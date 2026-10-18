/*
    Qt-only replacement for KDE's KRandom as used by liboscar (message and
    rendezvous cookies). Part of the harbour_icq_nina fork of Kopete's liboscar
    (GPL-2.0-or-later, like the rest of the library).
*/
#ifndef LIBOSCAR_COMPAT_KRANDOM_H
#define LIBOSCAR_COMPAT_KRANDOM_H

#include <random>

namespace KRandom {
// A non-negative pseudo-random int, like KRandom::random().
inline int random()
{
    static thread_local std::mt19937 engine{ std::random_device{}() };
    return static_cast<int>(engine() & 0x7fffffffu);
}
}

#endif
