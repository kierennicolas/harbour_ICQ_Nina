// Tests AccountManager (the app's core) against a running Open OSCAR Server
// with accounts 100001/pw100001 and 100002/pw100002 (see oos-integration.sh).
// Server: $OSCAR_TEST_HOST (default 127.0.0.1), port 5190, no TLS.
// Part of harbour_icq_nina; GPL-3.0-or-later.

#include <QtTest>
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QImage>
#include <QPainter>

#include "accountmanager.h"
#include "chatmodel.h"
#include "contactmodel.h"
#include "emoji.h"
#include "historystore.h"
#include "medialink.h"
#include "mediamanager.h"
#include "stickerstore.h"
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

    static QString assets(const QString &name)
    {
        return QStringLiteral(ICQNINA_SOURCE_DIR "/app/") + name;
    }

    static QVariant lastRow(ChatModel *chat, int role)
    {
        return chat->rowCount() > 0 ? chat->data(chat->index(chat->rowCount() - 1), role) : QVariant();
    }

    // A short WAV file (silence) standing in for a voice recording.
    QString makeWav(const QString &name, int ms)
    {
        const int rate = 8000;
        const QByteArray samples(rate * ms / 1000 * 2, '\0');
        QByteArray wav;
        QDataStream out(&wav, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);
        out.writeRawData("RIFF", 4); out << quint32(36 + samples.size());
        out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(1) << quint32(rate)
                                              << quint32(rate * 2) << quint16(2) << quint16(16);
        out.writeRawData("data", 4); out << quint32(samples.size());
        out.writeRawData(samples.constData(), samples.size());
        const QString path = m_files.path() + QLatin1Char('/') + name;
        QFile f(path);
        return f.open(QIODevice::WriteOnly) && f.write(wav) == wav.size() ? path : QString();
    }

private slots:
    void initTestCase()
    {
        m_host = QString::fromLatin1(qgetenv("OSCAR_TEST_HOST"));
        if (m_host.isEmpty())
            m_host = QStringLiteral("127.0.0.1");
        QVERIFY(m_dirA.isValid() && m_dirB.isValid() && m_dirC.isValid());
    }

    void mediaLinks()
    {
        MediaLink l = MediaLink::parse(QStringLiteral("Photo: https://litter.catbox.moe/ab12cd.jpg#icqnina-photo=1600x1200"));
        QCOMPARE(l.type, int(MediaLink::Photo));
        QCOMPARE(l.info, QStringLiteral("1600x1200"));
        QCOMPARE(l.url, QStringLiteral("https://litter.catbox.moe/ab12cd.jpg#icqnina-photo=1600x1200"));
        l = MediaLink::parse(QStringLiteral("Voice message (0:12): https://litter.catbox.moe/x.ogg#icqnina-voice=12300"));
        QCOMPARE(l.type, int(MediaLink::Voice));
        QCOMPARE(l.info, QStringLiteral("12300"));
        l = MediaLink::parse(QStringLiteral("Sticker: https://litter.catbox.moe/x.png#icqnina-sticker=hello"));
        QCOMPARE(l.type, int(MediaLink::Sticker));
        QCOMPARE(l.info, QStringLiteral("hello"));
        // A bad sticker id is dropped (the picture is still loaded).
        l = MediaLink::parse(QStringLiteral("Sticker: https://litter.catbox.moe/x.png#icqnina-sticker=../../etc"));
        QCOMPARE(l.type, int(MediaLink::Sticker));
        QVERIFY(l.info.isEmpty());
        // A bare link to a picture on the file host is a photo.
        QCOMPARE(MediaLink::parse(QStringLiteral(" https://files.catbox.moe/abc.PNG ")).type, int(MediaLink::Photo));
        // ...but not with other text, on another host, or two links.
        QCOMPARE(MediaLink::parse(QStringLiteral("look https://files.catbox.moe/abc.png")).type, int(MediaLink::None));
        QCOMPARE(MediaLink::parse(QStringLiteral("https://example.com/abc.png")).type, int(MediaLink::None));
        QCOMPARE(MediaLink::parse(QStringLiteral("Photo: https://example.com/a.jpg#icqnina-photo")).type, int(MediaLink::None));
        QCOMPARE(MediaLink::parse(QStringLiteral("https://litter.catbox.moe/a.jpg https://litter.catbox.moe/b.jpg")).type,
                 int(MediaLink::None));
        QCOMPARE(MediaLink::parse(QStringLiteral("Files (1 MB): https://litter.catbox.moe/a.zip")).type, int(MediaLink::None));

        QCOMPARE(MediaLink::messageText(MediaLink::Voice, QStringLiteral("https://litter.catbox.moe/v.ogg"), QStringLiteral("65400")),
                 QStringLiteral("Voice message (1:05): https://litter.catbox.moe/v.ogg#icqnina-voice=65400"));
        QCOMPARE(MediaLink::summary(MediaLink::Voice, QStringLiteral("9600")), QStringLiteral("Voice message (0:10)"));
        // What we send is understood again.
        const QString sent = MediaLink::messageText(MediaLink::Sticker, QStringLiteral("https://litter.catbox.moe/s.png"), QStringLiteral("u-0123abcd"));
        l = MediaLink::parse(sent);
        QCOMPARE(l.type, int(MediaLink::Sticker));
        QCOMPARE(l.info, QStringLiteral("u-0123abcd"));
    }

    void emojiText()
    {
        Emoji e;
        QVERIFY(e.load(assets(QStringLiteral("emoji"))));
        QVERIFY(e.categories().size() >= 5);
        const QString rich = e.richText(QString::fromUtf8("hi \xF0\x9F\x98\x80 <b>&\nsee https://example.com/a?b=1&c=2."), 20);
        QVERIFY2(rich.contains(QStringLiteral("1f600.png\" width=\"20\" height=\"20\"")), qPrintable(rich));
        QVERIFY2(rich.contains(QStringLiteral("&lt;b&gt;&amp;<br>see ")), qPrintable(rich));
        QVERIFY2(rich.contains(QStringLiteral("<a href=\"https://example.com/a?b=1&amp;c=2\">https://example.com/a?b=1&amp;c=2</a>.")),
                 qPrintable(rich));
        QCOMPARE(e.emojiOnly(QString::fromUtf8("\xF0\x9F\x98\x80\xF0\x9F\x98\x80")), 2);
        QCOMPARE(e.emojiOnly(QString::fromUtf8("\xE2\x9D\xA4\xEF\xB8\x8F")), 1);   // red heart with U+FE0F
        QCOMPARE(e.emojiOnly(QString::fromUtf8("a\xF0\x9F\x98\x80")), 0);
        QCOMPARE(e.emojiOnly(QString::fromUtf8("\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7")), 1);   // a flag: two code points
        QVERIFY(!e.urlOf(QString::fromUtf8("\xF0\x9F\x93\xB7")).isEmpty());
    }

    void stickerStore()
    {
        QTemporaryDir dir;
        StickerStore s;
        s.open(assets(QStringLiteral("stickers")), dir.path() + "/mine");
        QCOMPARE(s.rowCount(), 12);
        QCOMPARE(s.data(s.index(0), StickerStore::IdRole).toString(), QStringLiteral("hello"));
        QVERIFY(s.isBundled(QStringLiteral("hello")));
        QVERIFY(!s.remove(QStringLiteral("hello")));

        QImage big(600, 400, QImage::Format_ARGB32);
        big.fill(Qt::transparent);
        QPainter p(&big);
        p.setBrush(Qt::red);
        p.drawEllipse(100, 50, 400, 300);
        p.end();
        const QString path = dir.path() + "/cat.png";
        QVERIFY(big.save(path));
        const QString id = s.add(QUrl::fromLocalFile(path).toString());
        QVERIFY2(id.startsWith(QStringLiteral("u-")), qPrintable(s.lastError()));
        QCOMPARE(s.add(path), id); // the same picture again
        QCOMPARE(s.rowCount(), 13);
        const QImage stored(s.pathOf(id));
        QCOMPARE(stored.size(), QSize(256, 170));
        QVERIFY(stored.hasAlphaChannel());
        // Kept under the id it came with.
        QCOMPARE(s.add(path, QStringLiteral("u-feedfacecafe0001")), QStringLiteral("u-feedfacecafe0001"));
        QVERIFY(s.add(dir.path() + "/missing.png").isEmpty());

        s.setCachedLink(id, QStringLiteral("https://litter.catbox.moe/x.png"), 72);
        QCOMPARE(s.cachedLink(id), QStringLiteral("https://litter.catbox.moe/x.png"));
        s.setCachedLink(QStringLiteral("hello"), QStringLiteral("https://litter.catbox.moe/y.png"), 1);
        QVERIFY(s.cachedLink(QStringLiteral("hello")).isEmpty()); // expires too soon to send

        StickerStore again;
        again.open(assets(QStringLiteral("stickers")), dir.path() + "/mine");
        QCOMPARE(again.rowCount(), 14);
        QCOMPARE(again.cachedLink(id), QStringLiteral("https://litter.catbox.moe/x.png"));
        QVERIFY(again.remove(id));
        QCOMPARE(again.rowCount(), 13);
        QVERIFY(!QFile::exists(s.pathOf(id)));
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
    void profilePicture();
    void profileDetails();
    void media();
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
    QSignalSpy refused(&a, &AccountManager::contactEditFailed);
    QVERIFY2(a.addContact(QStringLiteral("100002"), QString(), QStringLiteral("It is me, from the test")),
             refused.isEmpty() ? "?" : qPrintable(refused.last().at(1).toString()));
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

void TestCore::profilePicture()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    // A needs B on the list to be told about B's picture.
    if (!a.isContact(QStringLiteral("100002"))) {
        a.addContact(QStringLiteral("100002"), QStringLiteral("Buddies"));
        QVERIFY(waitFor([&] { return a.isContact(QStringLiteral("100002")); }, 10000));
    }
    QImage image(400, 300, QImage::Format_RGB32);
    image.fill(QColor(30, 160, 110));
    QPainter p(&image);
    p.setBrush(Qt::white);
    p.drawEllipse(120, 70, 160, 160);
    p.end();
    const QString path = m_files.path() + "/me.jpg";
    QVERIFY(image.save(path, "JPEG"));

    QVERIFY(b.setOwnPicture(QUrl::fromLocalFile(path).toString()));
    QVERIFY(!b.ownPicture().isEmpty()); // shown straight away
    QVERIFY2(waitFor([&] { return !a.pictureOf(QStringLiteral("100002")).isEmpty(); }, 20000),
             "A never received B's picture");
    QImage received(QUrl(a.largePictureOf(QStringLiteral("100002"))).toLocalFile());
    QCOMPARE(received.size(), QSize(96, 96));

    b.removeOwnPicture();
    QVERIFY(b.ownPicture().isEmpty());
}

void TestCore::profileDetails()
{
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    QSignalSpy saved(&a, &AccountManager::profileSaved);
    QVariantMap f;
    f.insert(QStringLiteral("nickname"), QString::fromUtf8("Alïce"));
    f.insert(QStringLiteral("firstName"), QStringLiteral("Alice"));
    f.insert(QStringLiteral("lastName"), QStringLiteral("Tester"));
    f.insert(QStringLiteral("city"), QStringLiteral("Gibraltar"));
    f.insert(QStringLiteral("gender"), 1);
    f.insert(QStringLiteral("about"), QStringLiteral("Testing ICQ Nina"));
    QVERIFY(a.saveProfile(f));
    QVERIFY(a.savingProfile());
    QVERIFY(waitFor([&] { return saved.count() == 1; }, 15000));
    QVERIFY2(saved.first().at(0).toBool(), qPrintable(saved.first().at(1).toString()));

    QSignalSpy info(&b, &AccountManager::userInfoReceived);
    b.requestUserInfo(QStringLiteral("100001"));
    QVERIFY(waitFor([&] { return info.count() >= 1; }, 10000));
    const QVariantMap got = info.last().at(1).toMap();
    QCOMPARE(got.value(QStringLiteral("nickname")).toString(), QString::fromUtf8("Alïce"));
    QCOMPARE(got.value(QStringLiteral("firstName")).toString(), QStringLiteral("Alice"));
    QCOMPARE(got.value(QStringLiteral("city")).toString(), QStringLiteral("Gibraltar"));
    QCOMPARE(got.value(QStringLiteral("gender")).toInt(), 1);
}

void TestCore::media()
{
    if (qgetenv("ICQNINA_TEST_DOWNLOAD_BASE").isEmpty())
        QSKIP("ICQNINA_TEST_DOWNLOAD_BASE not set");
    AccountManager &a = *m_a;
    AccountManager &b = *m_b;
    a.setStickerPack(assets(QStringLiteral("stickers")));
    b.setStickerPack(assets(QStringLiteral("stickers")));
    // B loads media automatically only from people on B's list.
    if (!b.isContact(QStringLiteral("100001"))) {
        b.addContact(QStringLiteral("100001"), QStringLiteral("Buddies"));
        QVERIFY(waitFor([&] { return b.isContact(QStringLiteral("100001")); }, 10000));
    }
    ChatModel *aChat = a.chat(QStringLiteral("100002"));
    ChatModel *bChat = b.chat(QStringLiteral("100001"));
    QSignalSpy received(&b, &AccountManager::messageReceived);

    // A photo: made smaller, sent as a link, loaded by B.
    QImage photo(3200, 2400, QImage::Format_ARGB32);
    photo.fill(QColor(200, 120, 40, 200));
    const QString photoPath = m_files.path() + "/holiday.png";
    QVERIFY(photo.save(photoPath));
    QVERIFY2(a.media()->sendPhoto(QStringLiteral("100002"), QUrl::fromLocalFile(photoPath).toString()),
             qPrintable(a.media()->lastError()));
    QCOMPARE(lastRow(aChat, ChatModel::KindRole).toInt(), int(StoredMessage::Media));
    QCOMPARE(lastRow(aChat, ChatModel::MediaTypeRole).toInt(), int(MediaLink::Photo));
    QCOMPARE(lastRow(aChat, ChatModel::MediaInfoRole).toString(), QStringLiteral("1600x1200"));
    QVERIFY2(waitFor([&] { return lastRow(aChat, ChatModel::FileStateRole).toInt() == TransferManager::Done; }, 20000),
             qPrintable(a.media()->lastError()));
    QVERIFY(lastRow(aChat, ChatModel::FileUrlRole).toString().endsWith(QStringLiteral(".jpg#icqnina-photo=1600x1200")));
    QVERIFY(waitFor([&] { return lastRow(aChat, ChatModel::DeliveredRole).toBool(); }, 5000));
    const QString sentPhoto = lastRow(aChat, ChatModel::FilePathRole).toString();
    QCOMPARE(QImage(sentPhoto).size(), QSize(1600, 1200));

    QVERIFY(waitFor([&] { return received.count() >= 1; }, 10000));
    QCOMPARE(received.last().at(2).toString(), QStringLiteral("Photo"));
    QCOMPARE(lastRow(bChat, ChatModel::MediaTypeRole).toInt(), int(MediaLink::Photo));
    QVERIFY2(waitFor([&] { return lastRow(bChat, ChatModel::FileStateRole).toInt() == TransferManager::Done; }, 15000),
             qPrintable(b.media()->lastError()));
    const QString gotPhoto = lastRow(bChat, ChatModel::FilePathRole).toString();
    QVERIFY(gotPhoto.startsWith(b.media()->mediaDir()));
    QCOMPARE(contents(gotPhoto), contents(sentPhoto));
    QString preview;
    for (int i = 0; i < b.contacts()->rowCount(); ++i)
        if (b.contacts()->data(b.contacts()->index(i), ContactModel::UinRole).toString() == QLatin1String("100001"))
            preview = b.contacts()->data(b.contacts()->index(i), ContactModel::LastMessageRole).toString();
    QCOMPARE(preview, QStringLiteral("Photo"));

    // A voice message.
    const QString wav = makeWav(QStringLiteral("rec.wav"), 1300);
    QVERIFY2(a.media()->sendVoice(QStringLiteral("100002"), wav, 1300), qPrintable(a.media()->lastError()));
    QVERIFY(waitFor([&] { return received.count() >= 2; }, 20000));
    QCOMPARE(received.last().at(2).toString(), QStringLiteral("Voice message (0:01)"));
    QCOMPARE(lastRow(bChat, ChatModel::MediaInfoRole).toString(), QStringLiteral("1300"));
    QVERIFY(waitFor([&] { return lastRow(bChat, ChatModel::FileStateRole).toInt() == TransferManager::Done; }, 15000));
    QCOMPARE(contents(lastRow(bChat, ChatModel::FilePathRole).toString()), contents(wav));

    // One of the app's stickers: B has it, so nothing is downloaded.
    QVERIFY(a.media()->sendSticker(QStringLiteral("100002"), QStringLiteral("hello")));
    QVERIFY(waitFor([&] { return received.count() >= 3; }, 20000));
    QCOMPARE(lastRow(bChat, ChatModel::MediaTypeRole).toInt(), int(MediaLink::Sticker));
    QCOMPARE(lastRow(bChat, ChatModel::FileStateRole).toInt(), int(TransferManager::Done));
    QCOMPARE(lastRow(bChat, ChatModel::FilePathRole).toString(), b.media()->stickers()->pathOf(QStringLiteral("hello")));
    // Sent again: the link is reused, so it goes at once.
    QVERIFY(a.media()->sendSticker(QStringLiteral("100002"), QStringLiteral("hello")));
    QCOMPARE(lastRow(aChat, ChatModel::FileStateRole).toInt(), int(TransferManager::Done));
    QVERIFY(waitFor([&] { return received.count() >= 4; }, 10000));

    // A sticker of A's own: B downloads it and can keep it under its id.
    QImage art(300, 300, QImage::Format_ARGB32);
    art.fill(Qt::transparent);
    QPainter p(&art);
    p.setBrush(Qt::blue);
    p.drawEllipse(20, 20, 260, 260);
    p.end();
    QVERIFY(art.save(m_files.path() + "/art.png"));
    const QString mine = a.media()->stickers()->add(m_files.path() + "/art.png");
    QVERIFY(mine.startsWith(QStringLiteral("u-")));
    QVERIFY(a.media()->sendSticker(QStringLiteral("100002"), mine));
    QVERIFY(waitFor([&] { return received.count() >= 5; }, 20000));
    QCOMPARE(lastRow(bChat, ChatModel::MediaInfoRole).toString(), mine);
    QVERIFY(waitFor([&] { return lastRow(bChat, ChatModel::FileStateRole).toInt() == TransferManager::Done; }, 15000));
    QCOMPARE(b.media()->stickers()->add(lastRow(bChat, ChatModel::FilePathRole).toString(), mine), mine);
    QVERIFY(b.media()->stickers()->contains(mine));

    // Not loaded automatically when that is switched off; loaded on request.
    b.setAutoLoadMedia(false);
    QVERIFY(a.media()->sendPhoto(QStringLiteral("100002"), photoPath));
    QVERIFY(waitFor([&] { return received.count() >= 6; }, 20000));
    QTest::qWait(1000);
    QCOMPARE(lastRow(bChat, ChatModel::FileStateRole).toInt(), int(TransferManager::Offered));
    b.media()->load(QStringLiteral("100001"), lastRow(bChat, ChatModel::IdRole).toLongLong());
    QVERIFY(waitFor([&] { return lastRow(bChat, ChatModel::FileStateRole).toInt() == TransferManager::Done; }, 15000));
    b.setAutoLoadMedia(true);

    // An expired link.
    QSignalSpy failed(b.media(), &MediaManager::failed);
    QVERIFY(a.sendMessage(QStringLiteral("100002"), QStringLiteral("Photo: https://files.test/gone.jpg#icqnina-photo")));
    QVERIFY(waitFor([&] { return received.count() >= 7; }, 10000));
    QVERIFY(waitFor([&] { return lastRow(bChat, ChatModel::FileStateRole).toInt() == TransferManager::Failed; }, 15000));
    QCOMPARE(failed.last().at(1).toString(), QStringLiteral("The link has expired."));

    // History keeps it all.
    StoredMessage m = b.storedMessage(lastRow(bChat, ChatModel::IdRole).toLongLong());
    QCOMPARE(m.kind, int(StoredMessage::Media));
    QCOMPARE(m.fileState, int(TransferManager::Failed));
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
