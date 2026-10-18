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

class TestCore : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dirA, m_dirB, m_dirC;
    QString m_host;

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

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "minimal");
    QGuiApplication app(argc, argv);
    TestCore t;
    return QTest::qExec(&t, argc, argv);
}

#include "tst_core.moc"
