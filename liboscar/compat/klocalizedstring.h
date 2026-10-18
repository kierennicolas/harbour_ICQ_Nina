/*
    Qt-only replacement for KDE's i18n() as used by liboscar.
    Part of the harbour_icq_nina fork of Kopete's liboscar (GPL-2.0-or-later, like the rest of the library).
    The library's few user-visible strings are returned untranslated; the app
    shows its own translated messages.
*/
#ifndef LIBOSCAR_COMPAT_KLOCALIZEDSTRING_H
#define LIBOSCAR_COMPAT_KLOCALIZEDSTRING_H

#include <QString>

inline QString i18n(const char *text)
{
    return QString::fromUtf8(text);
}

template <typename A1>
inline QString i18n(const char *text, const A1 &a1)
{
    return QString::fromUtf8(text).arg(a1);
}

template <typename A1, typename A2>
inline QString i18n(const char *text, const A1 &a1, const A2 &a2)
{
    return QString::fromUtf8(text).arg(a1).arg(a2);
}

#endif
