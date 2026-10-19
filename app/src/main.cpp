// ICQ Nina: an ICQ/AIM client for Sailfish OS on the NINA revival service.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#include <sailfishapp.h>

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QtQml>

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

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    qmlRegisterUncreatableType<AccountManager>("harbour.icqnina", 1, 0, "Account", QStringLiteral("use the account context property"));
    qmlRegisterUncreatableType<ContactModel>("harbour.icqnina", 1, 0, "ContactModel", QStringLiteral("from account.contacts"));
    qmlRegisterUncreatableType<ChatModel>("harbour.icqnina", 1, 0, "ChatModel", QStringLiteral("from account.chat()"));
    qmlRegisterUncreatableType<SearchModel>("harbour.icqnina", 1, 0, "SearchModel", QStringLiteral("from account.searchResults"));
    qmlRegisterUncreatableType<TransferManager>("harbour.icqnina", 1, 0, "Transfers", QStringLiteral("from account.transfers"));
    qmlRegisterUncreatableType<MediaManager>("harbour.icqnina", 1, 0, "Media", QStringLiteral("from account.media"));
    qmlRegisterUncreatableType<StickerStore>("harbour.icqnina", 1, 0, "Stickers", QStringLiteral("from account.media.stickers"));
    qmlRegisterType<VoiceRecorder>("harbour.icqnina", 1, 0, "VoiceRecorder");

    // Under Sailjail these resolve to the app's own folders.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    AccountManager account(configDir, dataDir);
    account.setStickerPack(SailfishApp::pathTo(QStringLiteral("stickers")).toLocalFile());
    Emoji emoji;
    emoji.load(SailfishApp::pathTo(QStringLiteral("emoji")).toLocalFile());
    DBusService dbus;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("account"), &account);
    view->rootContext()->setContextProperty(QStringLiteral("dbus"), &dbus);
    view->rootContext()->setContextProperty(QStringLiteral("emoji"), &emoji);
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral(APP_VERSION));
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
