/*
    The KIO error codes liboscar's file transfer code reports, with KIO's
    numeric values. Part of the harbour_icq_nina fork of Kopete's liboscar
    (GPL-2.0-or-later, like the rest of the library).
*/
#ifndef LIBOSCAR_COMPAT_KIO_GLOBAL_H
#define LIBOSCAR_COMPAT_KIO_GLOBAL_H

namespace KIO {
enum Error {
    ERR_CANNOT_OPEN_FOR_READING = 1,
    ERR_CANNOT_OPEN_FOR_WRITING = 2,
    ERR_DOES_NOT_EXIST = 11,
    ERR_USER_CANCELED = 20,
    ERR_ABORTED = ERR_USER_CANCELED,
    ERR_COULD_NOT_CONNECT = 23,
    ERR_CONNECTION_BROKEN = 24,
    ERR_COULD_NOT_READ = 28,
    ERR_COULD_NOT_LISTEN = 31,
    ERR_COULD_NOT_ACCEPT = 32,
    ERR_COULD_NOT_LOGIN = 33
};
}

#endif
