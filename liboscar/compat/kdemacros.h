/*
    Symbol visibility macros replacing KDE's <kdemacros.h>.
    Part of the harbour_icq_nina fork of Kopete's liboscar (GPL-2.0-or-later, like the rest of the library).
*/
#ifndef LIBOSCAR_COMPAT_KDEMACROS_H
#define LIBOSCAR_COMPAT_KDEMACROS_H

#define KDE_EXPORT __attribute__((visibility("default")))
#define KDE_IMPORT __attribute__((visibility("default")))

#endif
