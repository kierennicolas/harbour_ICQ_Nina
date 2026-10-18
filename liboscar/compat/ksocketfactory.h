/*
    Qt-only replacement for KDE's KSocketFactory as used by liboscar's file
    transfer code. Part of the harbour_icq_nina fork of Kopete's liboscar
    (GPL-2.0-or-later, like the rest of the library).
*/
#ifndef LIBOSCAR_COMPAT_KSOCKETFACTORY_H
#define LIBOSCAR_COMPAT_KSOCKETFACTORY_H

#include <QNetworkProxy>
#include <QString>
#include <QTcpSocket>

namespace KSocketFactory {
inline void connectToHost(QTcpSocket *socket, const QString & /*protocol*/,
                          const QString &host, quint16 port)
{
    socket->connectToHost(host, port);
}

inline QNetworkProxy proxyForListening(const QString & /*protocol*/)
{
    return QNetworkProxy(QNetworkProxy::NoProxy);
}
}

#endif
