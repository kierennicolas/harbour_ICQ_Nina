// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "dbusservice.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDebug>

const char *DBusService::serviceName()
{
    return "io.github.kierennicolas.icqnina";
}

DBusService::DBusService(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    m_registered = bus.registerObject(QStringLiteral("/"), this, QDBusConnection::ExportScriptableSlots)
            && bus.registerService(QString::fromLatin1(serviceName()));
    if (!m_registered)
        qWarning() << "D-Bus: could not register" << serviceName() << bus.lastError().message();
}

void DBusService::openConversation(const QString &uin)
{
    emit openConversationRequested(uin);
}

void DBusService::activate()
{
    emit activateRequested();
}
