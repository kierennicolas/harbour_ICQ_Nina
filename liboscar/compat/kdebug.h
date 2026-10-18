/*
    Qt-only replacement for the parts of KDE's <kdebug.h> that liboscar uses.
    Part of the harbour_icq_nina fork of Kopete's liboscar (GPL-2.0-or-later, like the rest of the library).

    Output goes to the "liboscar" logging category. Debug messages are off by
    default; enable them with QT_LOGGING_RULES="liboscar.debug=true".
*/
#ifndef LIBOSCAR_COMPAT_KDEBUG_H
#define LIBOSCAR_COMPAT_KDEBUG_H

#include <QDebug>
#include <QLoggingCategory>
#include <QString>

Q_DECLARE_LOGGING_CATEGORY(lcOscar)

// The KDE debug "area" argument is ignored.
#define kDebug(...) qCDebug(lcOscar)
#define kWarning(...) qCWarning(lcOscar)
#define kError(...) qCCritical(lcOscar)
#define k_funcinfo Q_FUNC_INFO << ":"

inline QString kBacktrace() { return QString(); }

#endif
