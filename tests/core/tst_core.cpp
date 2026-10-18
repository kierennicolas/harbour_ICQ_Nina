// Tests AccountManager (the app's core) against a running Open OSCAR Server
// with accounts 100001/pw100001 and 100002/pw100002 (see oos-integration.sh).
// Server: $OSCAR_TEST_HOST (default 127.0.0.1), port 5190, no TLS.
// Part of harbour_icq_nina; GPL-3.0-or-later.

#include <QtTest>
#include <QGuiApplication>
#include <QTemporaryDir>

#include "accountmanager.h"
#include "chatmodel.h"
#include "contactmodel.h"
#include "historystore.h"
#include "searchmodel.h"
#include "transfermanager.h"

class TestCore : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dirA, m_dirB, m_dirC;
    QTemporaryDir m_p2a, m_p2b, m_files;
    QString m_host;
    AccountManager *m_a = nullptr;
    AccountManager *m_b = nullptr;

    QString makeFile(const QString &name, int size)
    {
        const QString path = m_files.path() + QLatin1Char('/') + name;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            return QString();
        QByteArray data(size, Qt::Uninitialized);
        for (int i = 0; i < size; ++i)
            data[i] = char((i * 131 + 7) % 251);
        f.write(data);
        return path;
    }

    static QByteArray contents(const QString &path)
    {
        QFile f(path);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    // Row of the file message for a transfer in a conversation.
    static int fileRow(ChatModel *chat, const QString &transferId)
    {
        for (int i = 0; i < chat->rowCount(); ++i)
            if (chat->data(chat->index(i), ChatModel::TransferIdRole).toString() == transferId)
                return i;
        return -1;
    }

    static bool waitFor(std::function<bool()> cond, int ms)
    {
        QElapsedTimer t;
        t.start();
        while (!cond() && t.elapsed() < ms)
            QTest::qWait(50);
        return cond();
    }

    void signIn(AccountManager &a, const QString &uin, const QString &pw)
    {
        a.signIn(uin, pw, true, QStringLiteral("custom"), m_host, 5190, false);
    }

private slots:
    void initTestCase()
    {
        m_host = QString::fromLatin1(qgetenv("OSCAR_TEST_HOST"));
        if (m_host.isEmpty())
            m_host = QStringLiteral("127.0.0.1");
        QVERIFY(m_dirA.isValid() && m_dirB.isValid() && m_dirC.isValid());
    }

    void wrongPasswordStopsWithoutRetrying()
    {
        AccountManager a(m_dirC.path() + "/config", m_dirC.path() + "/data");
        signIn(a, QStringLiteral("100001"), QStringLiteral("wrongpw1"));
        QVERIFY(waitFor([&] { return !a.lastError().isEmpty() && a.state() == AccountManager::Offline; }, 15000));
        QVERIFY2(a.lastError().contains(QStringLiteral("password")), qPrintable(a.lastError()));
        QCOMPARE(a.reconnectSeconds(), 0); // no automatic retry with a wrong password
        QTest::qWait(4000); // the server limits how fast one address may sign on
    }

    void messageBetweenTwoAccounts()
    {
        AccountManager a(m_dirA.path() + "/config", m_dirA.path() + "/data");
        AccountManager b(m_dirB.path() + "/config", m_dirB.path() + "/data");
        signIn(a, QStringLiteral("100001"), QStringLiteral("pw100001"));
        QVERIFY2(waitFor([&] { return a.state() == AccountManager::Online; }, 15000), qPrintable(a.lastError()));
        QTest::qWait(4000);
        signIn(b, QStringLiteral("100002"), QStringLiteral("pw100002"));
        QVERIFY2(waitFor([&] { return b.state() == AccountManager::Online; }, 15000), qPrintable(b.lastError()));

        b.setAppActive(false); // like the app in the background
        QSignalSpy received(&b, &AccountManager::messageReceived);
        const QString text = QString::fromUtf8("Hello from the core test ✓ Grüße 😀");
        ChatModel *aChat = a.chat(QStringLiteral("100002"));
        QVERIFY(a.sendMessage(QStringLiteral("100002"), text));
        QCOMPARE(aChat->rowCount(), 1);

        QVERIFY(waitFor([&] { return received.count() == 1; }, 10000));
        QCOMPARE(received.at(0).at(0).toString(), QStringLiteral("100001"));
        QCOMPARE(received.at(0).at(2).toString(), text);

        // Unread count and last message on the receiving side.
        QCOMPARE(b.unreadTotal(), 1);
        QVERIFY(b.contacts()->contains(QStringLiteral("100001")));
        ChatModel *bChat = b.chat(QStringLiteral("100001"));
        QCOMPARE(bChat->rowCount(), 1);
        QCOMPARE(bChat->data(bChat->index(0), ChatModel::TextRole).toString(), text);
        QCOMPARE(bChat->data(bChat->index(0), ChatModel::OutgoingRole).toBool(), false);

        // Delivery acknowledgement on the sending side.
        QVERIFY(waitFor([&] { return aChat->data(aChat->index(0), ChatModel::DeliveredRole).toBool(); }, 10000));

        // Opening the conversation marks it read.
        b.setAppActive(true);
        b.setActiveChat(QStringLiteral("100001"));
        QCOMPARE(b.unreadTotal(), 0);

        // A message to the open conversation is not counted as unread.
        QVERIFY(a.sendMessage(QStringLiteral("100002"), QStringLiteral("second")));
        QVERIFY(waitFor([&] { return bChat->rowCount() == 2; }, 10000));
        QCOMPARE(b.unreadTotal(), 0);

        // Status change while online keeps the session.
        a.setStatus(AccountManager::StatusAway, QStringLiteral("out for lunch"));
        QTest::qWait(1000);
        QCOMPARE(a.state(), int(AccountManager::Online));

        // Liveness check: the server answers our own-info request.
        a.keepAlive();
        QTest::qWait(3000);
        QCOMPARE(a.state(), int(AccountManager::Online));

        a.disconnectAccount();
        b.disconnectAccount();
        QCOMPARE(a.state(), int(AccountManager::Offline));
    }

    void phase2SignIn();
    void contactManagement();
    void authorization();
    void infoAndSearch();
    void blocking();
    void fileDirect();
    void fileDecline();
    void fileThroughProxy();
    void fileAsLink();
    void offlineMessage();
    void phase2SignOut();

    void historySurvivesRestart()
    {
        // A new AccountManager on B's folders reloads account and history.
        AccountManager b(m_dirB.path() + "/config", m_dirB.path() + "/data");
        QCOMPARE(b.uin(), QStringLiteral("100002"));
        QVERIFY(b.hasPassword());
        ChatModel *chat = b.chat(QStringLiteral("100001"));
        QCOMPARE(chat->rowCount(), 2);
        QVERIFY(b.contacts()->contains(QStringLiteral("100001")));
        b.forgetAccount();
        QVERIFY(!b.hasAccount());
    }
};

// ---- Phase 2 -----------------------------------------------------------------
// m_a = 100001, m_b = 100002 stay signed on across these tests; 100003 exists
// but never signs on.

void TestCore::phase2SignIn()
{
    QVERIFY(m_p2a.isValid() && m_p2b.isValid() && m_files.isValid());
    QTest::qWait(4000);
    m_a = new AccountManager(m_p2a.path() + "/config", m_p2a.path() + "/data", nullptr, m_p2a.path() + "/Downloads");
    m_b = new AccountManager(m_p2b.path() + "/config", m_p2b.path() + "/data", nullptr, m_p2b.path() + "/Downloads");
    m_a->setProxyTransfers(false);
    m_b->setProxyTransfers(false);
    signIn(*m_a, QStringLiteral("100001"), QStringLiteral("pw100001"));
    QVERIFY2(waitFor([&] { return m_a->state() == AccountManager::Online; }, 15000), qPrintable(m_a->lastError()));
    QTest::qWait(4000);
    signIn(*m_b, QStringLiteral("100002"), QStringLiteral("pw100002"));
    QVERIFY2(waitFor([&] { return m_b->state() == AccountManager::Online; }, 15000), qPrintable(m_b->lastError()));
    QTest::qWait(1500); // contact lists and own info
}

void TestCore::contactManagement()
{
    AccountManager &a = *m_a;
    QSignalSpy failed(&a, &AccountManager::contactEditFailed);
    // Into a group that does not exist yet: it is created first.
    QVERIFY(a.addContact(QStringLiteral("100003"), QStringLiteral("Friends")));
    QVERIFY2(waitFor([&] { return a.isContact(QStringLiteral("100003")); }, 10000),
             failed.isEmpty() ? "not added" : qPrintable(failed.first().at(1).toString()));
    QVERIFY(a.groups().contains(QStringLiteral("Friends")));
    QCOMPARE(a.groupOf(QStringLiteral("100003")), QStringLiteral("Friends"));
    QVERIFY(a.contacts()->contains(QStringLiteral("100003")));

    // Adding again is refused locally.
    QVERIFY(!a.addContact(QStringLiteral("100003"), QStringLiteral("Friends")));
    QCOMPARE(failed.count(), 1);

    a.renameContact(QStringLiteral("100003"), QStringLiteral("Charlie"));
    QVERIFY(waitFor([&] { return a.aliasOf(QStringLiteral("100003")) == QLatin1String("Charlie"); }, 10000));
    QVERIFY(waitFor([&] { return a.displayName(QStringLiteral("100003")) == QLatin1String("Charlie"); }, 5000));

    a.moveContact(QStringLiteral("100003"), QStringLiteral("Work"));
    QVERIFY(waitFor([&] { return a.groupOf(QStringLiteral("100003")) == QLatin1String("Work"); }, 15000));
    QCOMPARE(a.groupSize(QStringLiteral("Friends")), 0);

    QVERIFY(a.removeGroup(QStringLiteral("Friends")));
    QVERIFY(waitFor([&] { return !a.groups().contains(QStringLiteral("Friends")); }, 10000));
    QVERIFY(!a.removeGroup(QStringLiteral("Work"))); // not empty

    a.renameGroup(QStringLiteral("Work"), QStringLiteral("Colleagues"));
    QVERIFY(waitFor([&] { return a.groups().contains(QStringLiteral("Colleagues")); }, 10000));
    QCOMPARE(a.groupOf(QStringLiteral("100003")), QStringLiteral("Colleagues"));

    a.removeContact(QStringLiteral("100003"));
    QVERIFY(waitFor([&] { return !a.isContact(QStringLiteral("100003")); }, 10000));
    QVERIFY(!a.contacts()->contains(QStringLiteral("100003"))); // no history with them
    QVERIFY(a.removeGroup(QStringLiteral("Colleagues")));
}

void TestCore::authorization()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    b.setAuthorizationRequired(true);
    QTest::qWait(1500);

    QSignalSpy requested(&b, &AccountManager::authorizationRequested);
    QSignalSpy answered(&a, &AccountManager::authorizationAnswered);
    QVERIFY(a.addContact(QStringLiteral("100002"), QString(), QStringLiteral("It is me, from the test")));
    QVERIFY(waitFor([&] { return a.isContact(QStringLiteral("100002")); }, 10000));
    QVERIFY(waitFor([&] { return requested.count() >= 1; }, 10000));
    QCOMPARE(requested.first().at(0).toString(), QStringLiteral("100001"));
    QCOMPARE(requested.first().at(2).toString(), QStringLiteral("It is me, from the test"));

    b.answerAuthorization(QStringLiteral("100001"), true);
    QVERIFY(waitFor([&] { return answered.count() >= 1; }, 10000));
    QCOMPARE(answered.first().at(0).toString(), QStringLiteral("100002"));
    QCOMPARE(answered.first().at(1).toBool(), true);

    b.setAuthorizationRequired(false);
    QTest::qWait(1000);

    // Removing a contact you have talked to takes them off the list (the
    // history stays), and they come back when they write again.
    QVERIFY(a.sendMessage(QStringLiteral("100002"), QStringLiteral("before removing")));
    QTest::qWait(500);
    a.removeContact(QStringLiteral("100002"));
    QVERIFY(waitFor([&] { return !a.isContact(QStringLiteral("100002")); }, 10000));
    QVERIFY(waitFor([&] { return !a.contacts()->contains(QStringLiteral("100002")); }, 3000));
    QCOMPARE(a.chat(QStringLiteral("100002"))->rowCount(), 1);
    QVERIFY(b.sendMessage(QStringLiteral("100001"), QStringLiteral("I am still here")));
    QVERIFY(waitFor([&] { return a.contacts()->contains(QStringLiteral("100002")); }, 10000));
}

void TestCore::infoAndSearch()
{
    AccountManager &a = *m_a;
    QSignalSpy info(&a, &AccountManager::userInfoReceived);
    a.requestUserInfo(QStringLiteral("100002"));
    QVERIFY(waitFor([&] { return info.count() >= 1; }, 10000));
    QCOMPARE(info.first().at(0).toString(), QStringLiteral("100002"));

    QSignalSpy done(&a, &AccountManager::searchFinished);
    QVERIFY(a.searchByUin(QStringLiteral("100002")));
    QVERIFY(a.searching());
    QVERIFY(waitFor([&] { return done.count() == 1; }, 25000));
    QVERIFY(!a.searching());
    QCOMPARE(a.searchResults()->rowCount(), 1);
    QCOMPARE(a.searchResults()->data(a.searchResults()->index(0), SearchModel::UinRole).toString(), QStringLiteral("100002"));
}

void TestCore::blocking()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    a.block(QStringLiteral("100002"));
    QVERIFY(waitFor([&] { return a.isBlocked(QStringLiteral("100002")); }, 10000));
    QVERIFY(a.blockedContacts().contains(QStringLiteral("100002")));
    QVERIFY(waitFor([&] { return a.privacyMode() == AccountManager::BlockInvisibleList; }, 10000));

    QSignalSpy received(&a, &AccountManager::messageReceived);
    b.sendMessage(QStringLiteral("100001"), QStringLiteral("you should not see this"));
    QTest::qWait(3000);
    QCOMPARE(received.count(), 0);

    a.unblock(QStringLiteral("100002"));
    QVERIFY(waitFor([&] { return !a.isBlocked(QStringLiteral("100002")); }, 10000));
    QTest::qWait(500);
    b.sendMessage(QStringLiteral("100001"), QStringLiteral("unblocked now"));
    QVERIFY(waitFor([&] { return received.count() == 1; }, 10000));
    QCOMPARE(received.first().at(2).toString(), QStringLiteral("unblocked now"));
}

void TestCore::fileDirect()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    const QString path = makeFile(QStringLiteral("holiday photo.jpg"), 250000);
    QSignalSpy offers(b.transfers(), &TransferManager::incomingOffer);
    ChatModel *aChat = a.chat(QStringLiteral("100002"));
    ChatModel *bChat = b.chat(QStringLiteral("100001"));

    const QString id = a.transfers()->sendFile(QStringLiteral("100002"), QUrl::fromLocalFile(path).toString());
    QVERIFY2(!id.isEmpty(), qPrintable(a.transfers()->lastError()));
    QVERIFY(fileRow(aChat, id) >= 0);
    QVERIFY(waitFor([&] { return offers.count() == 1; }, 10000));
    QCOMPARE(offers.first().at(0).toString(), id);
    QCOMPARE(offers.first().at(2).toString(), QStringLiteral("holiday photo.jpg"));
    QCOMPARE(offers.first().at(3).toLongLong(), qint64(250000));
    QCOMPARE(b.transfers()->stateOf(id), int(TransferManager::Offered));
    QVERIFY(fileRow(bChat, id) >= 0);

    QVERIFY(b.transfers()->accept(id));
    QVERIFY2(waitFor([&] { return b.transfers()->stateOf(id) == TransferManager::Done; }, 20000),
             qPrintable(b.transfers()->errorOf(id)));
    QVERIFY2(waitFor([&] { return a.transfers()->stateOf(id) == TransferManager::Done; }, 10000),
             qPrintable(a.transfers()->errorOf(id)));
    const QString saved = bChat->data(bChat->index(fileRow(bChat, id)), ChatModel::FilePathRole).toString();
    QCOMPARE(saved, m_p2b.path() + "/Downloads/holiday photo.jpg");
    QCOMPARE(contents(saved), contents(path));
    QCOMPARE(bChat->data(bChat->index(fileRow(bChat, id)), ChatModel::FileStateRole).toInt(), int(TransferManager::Done));

    // A second file with the same name does not overwrite the first.
    const QString id2 = a.transfers()->sendFile(QStringLiteral("100002"), path);
    QVERIFY(waitFor([&] { return b.transfers()->stateOf(id2) == TransferManager::Offered; }, 10000));
    QVERIFY(b.transfers()->accept(id2));
    QVERIFY(waitFor([&] { return b.transfers()->stateOf(id2) == TransferManager::Done; }, 20000));
    QVERIFY(QFile::exists(m_p2b.path() + "/Downloads/holiday photo (1).jpg"));
}

void TestCore::fileDecline()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    const QString path = makeFile(QStringLiteral("unwanted.bin"), 1000);
    const QString id = a.transfers()->sendFile(QStringLiteral("100002"), path);
    QVERIFY(!id.isEmpty());
    QVERIFY(waitFor([&] { return b.transfers()->stateOf(id) == TransferManager::Offered; }, 10000));
    b.transfers()->decline(id);
    QCOMPARE(b.transfers()->stateOf(id), int(TransferManager::Declined));
    QVERIFY(waitFor([&] { return a.transfers()->stateOf(id) == TransferManager::Declined; }, 10000));
}

void TestCore::fileThroughProxy()
{
    const QByteArray ars = qgetenv("OSCAR_TEST_ARS"); // host:port of tests/fake-ars.py
    if (ars.isEmpty())
        QSKIP("OSCAR_TEST_ARS not set");
    const QString host = QString::fromLatin1(ars).section(QLatin1Char(':'), 0, 0);
    const int port = QString::fromLatin1(ars).section(QLatin1Char(':'), 1, 1).toInt();
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    a.setFileProxyOverride(host, port);
    b.setFileProxyOverride(host, port);
    a.setProxyTransfers(true);

    const QString path = makeFile(QStringLiteral("via proxy.pdf"), 400000);
    const QString id = a.transfers()->sendFile(QStringLiteral("100002"), path);
    QVERIFY(!id.isEmpty());
    QVERIFY(waitFor([&] { return b.transfers()->stateOf(id) == TransferManager::Offered; }, 10000));
    QVERIFY(b.transfers()->accept(id));
    QVERIFY2(waitFor([&] { return b.transfers()->stateOf(id) == TransferManager::Done; }, 20000),
             qPrintable(b.transfers()->errorOf(id)));
    QCOMPARE(contents(m_p2b.path() + "/Downloads/via proxy.pdf"), contents(path));
    a.setProxyTransfers(false);
}

void TestCore::fileAsLink()
{
    if (qgetenv("ICQNINA_TEST_UPLOAD_ENDPOINT").isEmpty())
        QSKIP("ICQNINA_TEST_UPLOAD_ENDPOINT not set");
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    QSignalSpy received(&b, &AccountManager::messageReceived);
    const QString path = makeFile(QStringLiteral("big video.mp4"), 300000);
    const QString id = a.transfers()->sendLink(QStringLiteral("100002"), path, 12);
    QVERIFY2(!id.isEmpty(), qPrintable(a.transfers()->lastError()));
    QCOMPARE(a.transfers()->stateOf(id), int(TransferManager::Uploading));
    QVERIFY2(waitFor([&] { return a.transfers()->stateOf(id) == TransferManager::Done; }, 20000),
             qPrintable(a.transfers()->errorOf(id)));
    QVERIFY(waitFor([&] { return received.count() >= 1; }, 10000));
    const QString text = received.last().at(2).toString();
    QVERIFY2(text.contains(QStringLiteral("https://files.test/")), qPrintable(text));
    QVERIFY(text.contains(QStringLiteral("big video.mp4")));
    ChatModel *aChat = a.chat(QStringLiteral("100002"));
    const int row = fileRow(aChat, id);
    QVERIFY(row >= 0);
    QVERIFY(aChat->data(aChat->index(row), ChatModel::FileUrlRole).toString().startsWith(QStringLiteral("https://files.test/")));
}

void TestCore::offlineMessage()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    b.disconnectAccount();
    QTest::qWait(1500);
    QVERIFY(a.sendMessage(QStringLiteral("100002"), QStringLiteral("sent while you were away")));
    QTest::qWait(4000);
    b.connectAccount();
    QVERIFY(waitFor([&] { return b.state() == AccountManager::Online; }, 15000));
    ChatModel *chat = b.chat(QStringLiteral("100001"));
    auto count = [&]() {
        int n = 0;
        for (int i = 0; i < chat->rowCount(); ++i)
            if (chat->data(chat->index(i), ChatModel::TextRole).toString() == QLatin1String("sent while you were away"))
                ++n;
        return n;
    };
    QVERIFY(waitFor([&] { return count() == 1; }, 15000));
    QTest::qWait(3000);
    QCOMPARE(count(), 1); // not twice
}

void TestCore::phase2SignOut()
{
    m_a->disconnectAccount();
    m_b->disconnectAccount();
    delete m_a;
    delete m_b;
    m_a = m_b = nullptr;
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "minimal");
    QGuiApplication app(argc, argv);
    TestCore t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_core.moc"
