# liboscar (harbour_icq_nina fork)

The OSCAR (ICQ/AIM) protocol library from KDE's Kopete instant messenger,
taken from <https://invent.kde.org/network/kopete> (GitHub mirror
<https://github.com/KDE/kopete>), `protocols/oscar/liboscar` at commit
`3ed5ea10e0798bf6c1a899948e3ce278646fdf14` (2024-06-23).

Licence: the files carry mixed headers. Most are LGPL-2.0/2.1-or-later
(`COPYING.LIB`), but several core files (for example `buffer.cpp`,
`rateclass.cpp`, the message, typing and file-transfer tasks and
`utils/rtf2html.h`) are GPL-2.0-or-later (`COPYING`). The library as a whole
is therefore distributed under the **GNU GPL, version 2 or later**, which is
compatible with the GPL-3.0-or-later app it is built for. `COPYING` is the
GPL-2 text, `COPYING.LIB` the LGPL-2.1 text, and `COPYING.kopete` Kopete's
original licence note (including its OpenSSL linking exception). Check each file's
header before reusing a file on its own.

## Changes from Kopete

- Builds with plain Qt 5 (5.6 or later) and qmake (`liboscar.pro`) instead of
  KDE Frameworks and CMake. The KDE APIs it used are replaced by small
  Qt-only headers in `compat/`: `kdebug.h` (logging category `liboscar`;
  enable debug output with `QT_LOGGING_RULES="liboscar.debug=true"`),
  `KLocalizedString`/`i18n`, `krandom.h`, `ksocketfactory.h`, `kio/global.h`,
  `kdemacros.h` and `oscar_export.h`.
- `QString::fromAscii`/`toAscii` (removed from Qt 5) replaced by the Latin-1
  equivalents in `client.cpp` and `oscarmessage.cpp`.
- **Bug fix** in `coreprotocol.cpp`: a FLAP frame was considered complete when
  the buffer held `length` bytes instead of `6 + length`, so a frame whose
  last bytes had not arrived yet was parsed truncated and the stream lost
  sync. It showed up as a sign-on that hung when the server's 10-byte hello
  arrived in two TCP segments.
- `CMakeLists.txt` files removed.
