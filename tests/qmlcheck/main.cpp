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

#include "accountmanager.h"
#include "chatmodel.h"
#include "contactmodel.h"
#include "dbusservice.h"

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

    QTemporaryDir tmp;
    AccountManager account(tmp.path() + QStringLiteral("/config"), tmp.path() + QStringLiteral("/data"));
    DBusService dbus;

    QQuickView view;
    QQmlContext *ctx = view.rootContext();
    ctx->setContextProperty(QStringLiteral("account"), &account);
    ctx->setContextProperty(QStringLiteral("dbus"), &dbus);
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

    // Let bindings, timers and deferred creation run for a moment.
    QTimer::singleShot(1500, &app, &QCoreApplication::quit);
    app.exec();

    fprintf(stderr, "qmlcheck: %d problem(s) in app QML\n", g_problems);
    fflush(stderr);
    _exit(g_problems ? 1 : 0);
}
