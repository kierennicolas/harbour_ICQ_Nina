// D-Bus entry point so a tapped notification can open its conversation.
// Service and interface: io.github.kierennicolas.icqnina, object path /.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef DBUSSERVICE_H
#define DBUSSERVICE_H

#include <QObject>

class DBusService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.kierennicolas.icqnina")

public:
    static const char *serviceName();
    explicit DBusService(QObject *parent = nullptr);
    bool registered() const { return m_registered; }

public slots:
    Q_SCRIPTABLE void openConversation(const QString &uin);
    Q_SCRIPTABLE void activate();

signals:
    void openConversationRequested(const QString &uin);
    void activateRequested();

private:
    bool m_registered = false;
};

#endif
