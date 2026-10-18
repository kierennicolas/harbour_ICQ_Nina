# harbour_icq_nina

An ICQ-first instant messenger for **Sailfish OS** (target: Sony Xperia XA2,
Sailfish OS 4.5–5.x, Qt 5.6) that connects to the **NINA** revival service
([nina.chat](https://nina.chat)) over the classic OSCAR protocol, with AIM
screen names as a second login type.

Unofficial: not affiliated with NINA/Level Leap, ICQ or AOL.

## Status: Phase 0 (protocol spike)

| Piece | State |
| --- | --- |
| `liboscar/` | Kopete's OSCAR library, de-KDE'd: builds with plain Qt 5 and qmake (desktop Qt 5.15 and Sailfish Qt 5.6). Includes a fix for a stream-parsing bug that made sign-on hang. |
| `tools/oscarprobe/` | Command-line client: sign on, read the server-side contact list, presence, send/receive messages, report errors. |
| `tests/oos-integration.sh` | Signs on to a local [Open OSCAR Server](https://github.com/mk6i/open-oscar-server) with real password checks: wrong password, ICQ sign-on, contact list, own info, Unicode message with delivery acknowledgement, away status. |
| CI | Desktop build + integration tests; Sailfish SDK armv7hl build with the ARM binaries signing on and messaging under qemu; live NINA sign-on probe. |
| Sailfish app (Silica UI) | Phase 1, next. |

The full research behind the design (NINA's endpoints and feature status,
OSCAR in detail, which code to reuse, Sailfish constraints, phased plan and
risks) is in [`docs/research.md`](docs/research.md).

## Live NINA probe

The `nina` CI job signs on to `login.oscar.nina.chat:5190` (and the TLS
variant `slogin.oscar.nina.chat:5190`) with a test account. Add two
repository secrets under **Settings → Secrets and variables → Actions**:

- `NINA_UIN` – the ICQ number
- `NINA_PASSWORD` – its password

Without them the job is skipped with a warning. The probe's output is
redacted (other users' numbers masked, message texts hidden) because this
repository's logs are public.

## Build and run locally

```sh
mkdir build && cd build
qmake ../harbour_icq_nina.pro && make
OSCAR_PASSWORD=... tools/oscarprobe/oscarprobe --uin 123456 --wait 30
```

`oscarprobe --help` lists the options (server, TLS, client identity, status,
sending a message). Set `QT_LOGGING_RULES="liboscar.debug=true"` for a full
protocol trace.

## Licence

The app and tools are GPL-3.0-or-later (`LICENSE`). `liboscar/` keeps
Kopete's per-file licences (mixed LGPL-2+/GPL-2+, so GPL-2.0-or-later as a
whole); see [`liboscar/README.md`](liboscar/README.md).
