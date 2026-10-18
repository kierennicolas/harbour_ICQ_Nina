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

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setApplicationVersion(QStringLiteral(APP_VERSION));

    qmlRegisterUncreatableType<AccountManager>("harbour.icqnina", 1, 0, "Account", QStringLiteral("use the account context property"));
    qmlRegisterUncreatableType<ContactModel>("harbour.icqnina", 1, 0, "ContactModel", QStringLiteral("from account.contacts"));
    qmlRegisterUncreatableType<ChatModel>("harbour.icqnina", 1, 0, "ChatModel", QStringLiteral("from account.chat()"));

    // Under Sailjail these resolve to the app's own folders.
    const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    AccountManager account(configDir, dataDir);
    DBusService dbus;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("account"), &account);
    view->rootContext()->setContextProperty(QStringLiteral("dbus"), &dbus);
    view->rootContext()->setContextProperty(QStringLiteral("appVersion"), QStringLiteral(APP_VERSION));
    view->setSource(SailfishApp::pathToMainQml());
    view->show();
    return app->exec();
}
