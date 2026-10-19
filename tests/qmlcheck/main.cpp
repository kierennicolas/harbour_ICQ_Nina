// qmlcheck <app qml dir>
// 1. Compiles every QML file of the app against the real Sailfish modules
//    (catches missing imports/types, misspelt properties and handlers).
// 2. Starts the whole app UI (ApplicationWindow, contact list, cover,
//    notification and keep-alive objects) with a real offline AccountManager.
// 3. Creates every other page and dialog on its own.
// Warnings and errors that point at the app's files are counted.
// Part of harbour_icq_nina; GPL-3.0-or-later.

#include <QDir>
#include <QDirIterator>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#include <QDateTime>

#include "accountmanager.h"
#include "chatmodel.h"
#include "contactmodel.h"
#include "dbusservice.h"
#include "emoji.h"
#include "mediamanager.h"
#include "searchmodel.h"
#include "stickerstore.h"
#include "transfermanager.h"
#include "voicerecorder.h"

static QString g_appUrl;
static QString g_appPath;
static int g_problems = 0;

static void handler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    const bool ours = msg.contains(g_appUrl) || msg.contains(g_appPath);
    const bool counts = ours && type != QtDebugMsg && type != QtInfoMsg;
    if (counts)
        ++g_problems;
    fprintf(stderr, "%s%s\n", counts ? "[APP-QML] " : "          ", qPrintable(msg));
    fflush(stderr);
    if (type == QtFatalMsg)
        abort();
}

static QObject *create(QQmlEngine *engine, QQmlContext *ctx, const QDir &root, const char *file,
                       const QVariantMap &props = QVariantMap())
{
    QQmlComponent c(engine, QUrl::fromLocalFile(root.filePath(QString::fromLatin1(file))));
    QObject *o = c.beginCreate(ctx);
    if (!o) {
        for (const QQmlError &e : c.errors())
            qWarning("%s", qPrintable(e.toString()));
        ++g_problems;
        return nullptr;
    }
    for (auto it = props.constBegin(); it != props.constEnd(); ++it)
        o->setProperty(it.key().toUtf8().constData(), it.value());
    c.completeCreate();
    fprintf(stderr, "CREATED   %s (%s)\n", file, o->metaObject()->className());
    return o;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        fprintf(stderr, "usage: qmlcheck <app qml dir>\n");
        return 2;
    }
    const QDir root(QString::fromLocal8Bit(argv[1]));
    g_appPath = root.absolutePath();
    g_appUrl = QUrl::fromLocalFile(g_appPath).toString();
    qInstallMessageHandler(handler);

    qmlRegisterUncreatableType<AccountManager>("harbour.icqnina", 1, 0, "Account", QStringLiteral("no"));
    qmlRegisterUncreatableType<ContactModel>("harbour.icqnina", 1, 0, "ContactModel", QStringLiteral("no"));
    qmlRegisterUncreatableType<ChatModel>("harbour.icqnina", 1, 0, "ChatModel", QStringLiteral("no"));
    qmlRegisterUncreatableType<SearchModel>("harbour.icqnina", 1, 0, "SearchModel", QStringLiteral("no"));
    qmlRegisterUncreatableType<TransferManager>("harbour.icqnina", 1, 0, "Transfers", QStringLiteral("no"));
    qmlRegisterUncreatableType<MediaManager>("harbour.icqnina", 1, 0, "Media", QStringLiteral("no"));
    qmlRegisterUncreatableType<StickerStore>("harbour.icqnina", 1, 0, "Stickers", QStringLiteral("no"));
    qmlRegisterType<VoiceRecorder>("harbour.icqnina", 1, 0, "VoiceRecorder");

    QTemporaryDir tmp;
    AccountManager account(tmp.path() + QStringLiteral("/config"), tmp.path() + QStringLiteral("/data"));
    // The installed sticker pack and emoji (next to the qml folder).
    account.setStickerPack(root.absoluteFilePath(QStringLiteral("../stickers")));
    Emoji emoji;
    if (!emoji.load(root.absoluteFilePath(QStringLiteral("../emoji")))) {
        fprintf(stderr, "[APP-QML] emoji not installed next to %s\n", qPrintable(g_appPath));
        ++g_problems;
    }
    fprintf(stderr, "ASSETS    %d stickers, %d emoji categories\n", account.media()->stickers()->rowCount(),
            emoji.categories().size());
    if (account.media()->stickers()->rowCount() < 12) {
        fprintf(stderr, "[APP-QML] sticker pack not installed\n");
        ++g_problems;
    }
    DBusService dbus;

    QQuickView view;
    QQmlContext *ctx = view.rootContext();
    ctx->setContextProperty(QStringLiteral("account"), &account);
    ctx->setContextProperty(QStringLiteral("dbus"), &dbus);
    ctx->setContextProperty(QStringLiteral("emoji"), &emoji);
    ctx->setContextProperty(QStringLiteral("appVersion"), QStringLiteral("qmlcheck"));

    int files = 0;
    QDirIterator it(root.absolutePath(), QStringList() << QStringLiteral("*.qml"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        QQmlComponent c(view.engine(), QUrl::fromLocalFile(path));
        ++files;
        if (c.isError()) {
            for (const QQmlError &e : c.errors())
                qWarning("%s", qPrintable(e.toString()));
            ++g_problems;
        } else {
            fprintf(stderr, "COMPILED  %s\n", qPrintable(root.relativeFilePath(path)));
        }
    }
    fprintf(stderr, "compiled %d file(s)\n", files);

    view.setSource(QUrl::fromLocalFile(root.filePath(QStringLiteral("harbour-icqnina.qml"))));
    if (view.status() != QQuickView::Ready || !view.rootObject()) {
        for (const QQmlError &e : view.errors())
            qWarning("%s", qPrintable(e.toString()));
        ++g_problems;
    } else {
        fprintf(stderr, "CREATED   harbour-icqnina.qml (%s)\n", view.rootObject()->metaObject()->className());
    }

    create(view.engine(), ctx, root, "pages/SignInDialog.qml");
    create(view.engine(), ctx, root, "pages/StatusPage.qml");
    create(view.engine(), ctx, root, "pages/SettingsPage.qml");
    create(view.engine(), ctx, root, "pages/AboutPage.qml");
    create(view.engine(), ctx, root, "pages/ConversationPage.qml", { { QStringLiteral("uin"), QStringLiteral("100002") } });
    create(view.engine(), ctx, root, "pages/AuthorizationDialog.qml",
           { { QStringLiteral("uin"), QStringLiteral("100002") }, { QStringLiteral("name"), QStringLiteral("Test") },
             { QStringLiteral("reason"), QStringLiteral("Please add me") } });
    create(view.engine(), ctx, root, "cover/CoverPage.qml");
    // Phase 2 pages and components.
    const QString uin = QStringLiteral("100002");
    create(view.engine(), ctx, root, "pages/AddContactPage.qml");
    create(view.engine(), ctx, root, "pages/AddContactDialog.qml",
           { { QStringLiteral("uin"), uin }, { QStringLiteral("name"), QStringLiteral("Test") },
             { QStringLiteral("authRequired"), true } });
    create(view.engine(), ctx, root, "pages/ContactInfoPage.qml", { { QStringLiteral("uin"), uin } });
    create(view.engine(), ctx, root, "pages/PrivacyPage.qml");
    create(view.engine(), ctx, root, "pages/GroupsPage.qml");
    create(view.engine(), ctx, root, "pages/GroupDialog.qml", { { QStringLiteral("current"), QStringLiteral("Buddies") } });
    create(view.engine(), ctx, root, "pages/TextInputDialog.qml",
           { { QStringLiteral("title"), QStringLiteral("Rename") }, { QStringLiteral("label"), QStringLiteral("Name") } });
    create(view.engine(), ctx, root, "pages/SendFileDialog.qml",
           { { QStringLiteral("uin"), uin }, { QStringLiteral("name"), QStringLiteral("Test") },
             { QStringLiteral("filePath"), QStringLiteral("/tmp/photo.jpg") }, { QStringLiteral("fileName"), QStringLiteral("photo.jpg") },
             { QStringLiteral("fileSize"), 123456 } });
    for (int state = 0; state <= 7; ++state) {
        create(view.engine(), ctx, root, "components/FileBubble.qml",
               { { QStringLiteral("outgoing"), state % 2 == 1 }, { QStringLiteral("fileName"), QStringLiteral("photo.png") },
                 { QStringLiteral("fileSize"), 123456 }, { QStringLiteral("fileState"), state },
                 { QStringLiteral("fileDone"), 1000 },
                 // A picture that exists (received pictures are previewed).
                 { QStringLiteral("filePath"), QStringLiteral("/usr/share/harbour-icqnina/stickers/hello.png") },
                 { QStringLiteral("fileUrl"), state == 3 ? QStringLiteral("https://litter.catbox.moe/x.jpg") : QString() },
                 { QStringLiteral("transferId"), state == 3 ? QStringLiteral("link-1") : QStringLiteral("abc") },
                 { QStringLiteral("timestamp"), QDateTime::currentDateTime() },
                 { QStringLiteral("contact"), uin }, { QStringLiteral("contactName"), QStringLiteral("Test") } });
    }

    // 0.3: pictures, profile, photos, voice messages, stickers, emoji.
    create(view.engine(), ctx, root, "pages/ProfilePage.qml");
    create(view.engine(), ctx, root, "pages/ImageViewerPage.qml",
           { { QStringLiteral("source"), QStringLiteral("file:///usr/share/harbour-icqnina/stickers/hello.png") },
             { QStringLiteral("filePath"), QStringLiteral("/usr/share/harbour-icqnina/stickers/hello.png") },
             { QStringLiteral("link"), QStringLiteral("https://litter.catbox.moe/x.png#icqnina-photo") } });
    create(view.engine(), ctx, root, "pages/PhotoSendDialog.qml",
           { { QStringLiteral("filePath"), QStringLiteral("/usr/share/harbour-icqnina/stickers/wow.png") },
             { QStringLiteral("name"), QStringLiteral("Test") } });
    create(view.engine(), ctx, root, "components/Avatar.qml",
           { { QStringLiteral("name"), QStringLiteral("Test") }, { QStringLiteral("uin"), uin }, { QStringLiteral("presence"), 0 } });
    if (QObject *panel = create(view.engine(), ctx, root, "components/EmojiPanel.qml"))
        panel->setProperty("stickers", true);
    for (int type = 1; type <= 3; ++type) {
        for (int state : { 0, 2, 3, 4, 7 }) {
            const bool out = state == 7 || (state == 3 && type != 1);
            create(view.engine(), ctx, root, "components/MediaBubble.qml",
                   { { QStringLiteral("outgoing"), out }, { QStringLiteral("mediaType"), type },
                     { QStringLiteral("mediaInfo"), type == 1 ? QStringLiteral("1600x1200") : type == 2 ? QStringLiteral("12300") : QStringLiteral("hello") },
                     { QStringLiteral("fileState"), state }, { QStringLiteral("fileDone"), 1000 }, { QStringLiteral("fileSize"), 4000 },
                     { QStringLiteral("filePath"), state == 3 || out ? QStringLiteral("/usr/share/harbour-icqnina/stickers/hello.png") : QString() },
                     { QStringLiteral("fileUrl"), QStringLiteral("https://litter.catbox.moe/x.png#icqnina-sticker=hello") },
                     { QStringLiteral("messageId"), 1 }, { QStringLiteral("timestamp"), QDateTime::currentDateTime() },
                     { QStringLiteral("contact"), uin }, { QStringLiteral("maxWidth"), 400 } });
        }
    }
    // Message text with emoji and a link, as the conversation shows it.
    fprintf(stderr, "EMOJI     %s\n", qPrintable(emoji.richText(QString::fromUtf8("Hi \xF0\x9F\x98\x80 https://nina.chat"), 32)));

    // Deferred actions must still work after the menu that started them is
    // gone: run the contact list's remove action through its undo countdown.
    if (QObject *contacts = create(view.engine(), ctx, root, "pages/ContactsPage.qml")) {
        const bool ok = QMetaObject::invokeMethod(contacts, "removeEntry",
            Q_ARG(QVariant, QStringLiteral("100003")), Q_ARG(QVariant, QStringLiteral("Test")), Q_ARG(QVariant, false));
        fprintf(stderr, "INVOKED   ContactsPage.removeEntry: %s\n", ok ? "ok" : "FAILED");
        if (!ok)
            ++g_problems;
    }

    // Let bindings, timers, deferred creation and the countdown run.
    QTimer::singleShot(7000, &app, &QCoreApplication::quit);
    app.exec();

    fprintf(stderr, "qmlcheck: %d problem(s) in app QML\n", g_problems);
    fflush(stderr);
    _exit(g_problems ? 1 : 0);
}
