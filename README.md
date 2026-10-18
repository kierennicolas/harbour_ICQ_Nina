# harbour_icq_nina

An ICQ-first instant messenger for **Sailfish OS** (target: Sony Xperia XA2,
Sailfish OS 4.5–5.x, Qt 5.6) that connects to the **NINA** revival service
([nina.chat](https://nina.chat)) over the classic OSCAR protocol, with AIM
screen names as a second login type.

Unofficial: not affiliated with NINA/Level Leap, ICQ or AOL.

## Status

**Phase 0 (protocol) – done.** Signs on to NINA with an ICQ number over
plain and TLS connections, reads the server-side contact list and sends and
receives messages (verified in CI with a test account).

**Phase 1 (app) – done.** The Sailfish app *ICQ Nina*:

- sign in with an ICQ number (or AIM screen name) and password; NINA
  encrypted (default), NINA unencrypted or any other OSCAR server
- server-side contact list with groups, presence dots, unread counts, last
  message and typing indicator
- conversations with delivery ticks, history in SQLite, Unicode
- status (online, free for chat, away, N/A, occupied, DND, invisible) with
  a status message
- notifications per conversation (tap opens the chat), unread count on the
  cover
- automatic reconnect with backoff, liveness checks every 2.5 minutes even
  when the phone sleeps, optional "stay connected with the screen off"

**Phase 2 (ICQ complete) – first version (0.2.0).**

- add contacts: search the ICQ directory by number, nickname, name or email,
  or add a number directly; choose a group; if they require authorization the
  request (with your message) is sent automatically
- contact details from the directory; rename, move between groups, remove;
  manage groups (create, rename, delete empty ones)
- authorization both ways: answer requests (queued, with notifications),
  ask again, "others must ask before adding me"
- privacy: block (ignore + invisible lists), always/never visible to someone,
  who can see me online (everyone except blocked, only contacts, only the
  visible list, everyone, nobody)
- offline messages (ICQ meta retrieval on NINA, 04/10 elsewhere), with
  duplicate protection
- **files**: send from the file picker as an OSCAR file transfer (through
  NINA's relay by default, or directly) or as a Litterbox download link (works
  with any client and when they are offline); receive files (accept or decline,
  saved in Downloads, open from the chat); progress, cancel, "send as link
  instead" when a transfer fails

Verified in CI against a local Open OSCAR Server (contacts, groups,
authorization, blocking, directory, offline messages, direct and proxied file
transfers, links via a stand-in upload server) and against NINA (directory
search and info, ICQ offline retrieval, the rendezvous proxy handshake).

Next (Phase 3): AIM extras, multiple accounts, buddy icons, chat rooms,
share-to-app.

| Piece | What it is |
| --- | --- |
| `liboscar/` | Kopete's OSCAR library, de-KDE'd: plain Qt 5 + qmake (desktop Qt 5.15 and Sailfish Qt 5.6), with fixes for a stream-parsing bug and NINA's sign-on behaviour. Shipped privately in the RPM. |
| `app/` | The Sailfish app: a pure-Qt core (`app/src/core`: account, contacts, chats, history) and the Silica UI (`app/qml`). |
| `tools/oscarprobe/` | Command-line client for testing servers. |
| `tests/` | Integration tests against a local [Open OSCAR Server](https://github.com/mk6i/open-oscar-server) (probe and app core) and the real-Silica QML check. |

## Install on the phone (Xperia XA2)

Download `harbour-icqnina-*.armv7hl.rpm` from the latest
[release](../../releases/latest), or from the artifacts of a
[Build and test](../../actions/workflows/build.yml) run. Allow untrusted
software in Settings, then open the RPM, or run
`devel-su pkcon install-local harbour-icqnina-*.armv7hl.rpm`.

Sign in with your NINA ICQ number. If you set a separate *legacy password*
for old clients at account.escargot.chat, use that one.

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
