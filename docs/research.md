# Rebuild ICQ for Sailfish on NINA's servers

The best way to build this app is a native Qt 5.6 C++/Silica application wrapped around a de-KDE'd fork of **Kopete's LGPL-licensed liboscar**. It would speak classic OSCAR with BUCP/MD5 login to **`login.oscar.nina.chat:5190`**, or over TLS to `slogin.oscar.nina.chat:5190`. Pidgin's actively maintained **retro-prpl** serves as the behavioural reference, the MIT-licensed **Open OSCAR Server** as a local test server, and the app ships first through **Sailfish:Chum and OpenRepos**. NINA is a live AIM/ICQ revival run by Level Leap. It accepts ICQ clients from 2000a through 8.x and showed several hundred users online on 18 October 2026. Its server is in-house code that is not public, though, and ICQ support is officially beta. Offline messages, authorization requests, avatars and extended statuses are each either unfinished or unconfirmed, so the client has to probe features at runtime and degrade gracefully when one is missing. No maintained ICQ client exists for Sailfish OS; the last attempt was a testing-only Telepathy plugin from 2017. Kopete's protocol code already declares Qt 5.6.0 as its minimum, which matches the Qt 5.6 that Sailfish OS 5 still ships. The protocol itself is the easy part, because three independent open-source codebases describe it the same way. The hard parts are matching an undocumented beta server's behaviour and keeping a TCP session alive on a phone with no push service. Your existing ICQ number carries over only if it is registered on NINA. NINA imported nothing from AOL or ICQ, so an original UIN must be typed into NINA's registration form, and numbers go first-come-first-served. NINA has been fragile since its September 2025 Discord deletion and "legal reasons" web shutdown, so make the server host configurable and keep the client working against Open OSCAR Server as well.

## NINA is live, closed in practice, and still calls ICQ beta

### A Level Leap service that survived its 2025 crisis

NINA describes itself as rebuilding classic 1990s online services "as close to the originals as possible". That covers AIM in all versions including the mobile apps, **ICQ 2000a through 8.x** (described as "still under heavy development", with warnings that non-messaging features may break), and Classic AOL in alpha. Q-Link is planned, and MSN is left to the sister service Escargot ([nina.chat](https://nina.chat)). The Register named the backer as Level Leap, "a medical software company specializing in the cloud", with "Tony" speaking for the project ([The Register](https://www.theregister.com/2024/05/31/nina_saving_icq/)). Escargot and NINA share one login system and one account database ([NINA Wiki Extended FAQ](https://wiki.nina.chat/wiki/Support/Extended_FAQ)).

The About page says the project is open source at gitlab.com/hollerith ([nina.chat/about](https://nina.chat/about/)). Staff contradicted that in September 2025: the code still needs refactoring before release because it is built around their infrastructure ([MessengerGeek](https://wink.messengergeek.com/t/nina-escargot-discord-is-gone/26325)). The one visible GitLab project, "COOL", is described as "C# OSCAR and Other Libraries" ([GitLab COOL](https://gitlab.com/hollerith/cool/-/blob/main/docs/protocols/FLAP/README.md)). So NINA is a proprietary C# server, not Open OSCAR Server or any other public codebase, and its quirks can only be learned by testing against it.

The history explains where ICQ stands today. In **late 2023** NINA was a donation-gated private alpha with AIM up to 5.9 working ([VOGONS](https://www.vogons.org/viewtopic.php?p=1155521)). By **May 2024** it claimed ICQ 2000 through 8, still behind paid tiers ([The Register](https://www.theregister.com/2024/05/31/nina_saving_icq/)). ICQ Server Relay messaging arrived in **September 2024** ([NINA news 2024-09-06](https://nina.chat/news/120500000101220/this-week-in-updates-2024-09-06/)), and AIM became free in **November 2024** ([MessengerGeek](https://wink.messengergeek.com/t/aim-for-nina-escargot-now-public-and-free-to-use/25246)). ICQ went "open alpha", free "with a UIN only", most likely in **July 2025**; the post is undated, so that date is inferred from its position in the news feed ([NINA news: ICQ open alpha](https://nina.chat/news/120500000101270/icq-now-in-open-alpha-skype-news-and-other-stuff/)). In **September 2025** Tony deleted the roughly 13,000-member Discord after illegal material was reported against it, removed PayPal, and promised "The service will not be shutting down" ([NINA news: Why we shut down the Discord](https://nina.chat/news/120500000101290/why-we-shut-down-the-discord/)). Days later NINA said "for legal reasons, we had to shut a lot of things down", including the website, the wiki and most social media ([NINA news: What happens next](https://nina.chat/news/120500000101300/what-happens-next/)).

The cause of that shutdown is disputed. An anonymous insider thread alleged cease-and-desist letters, and staff member Animadoria replied "We never got an explicit cease and desist". The same thread contains an unverified claim that more than 1,000 accounts found in infostealer logs were suspended ([MessengerGeek "from the inside"](https://wink.messengergeek.com/t/nina-escargot-from-the-inside/26336)).

As of October 2026 the website, wiki and donation pages are back. The donation tiers were edited on 2026-10-07 ([nina.chat/donate](https://nina.chat/donate/)), and the home page counter read **603 online** ([nina.chat](https://nina.chat)). A July 2026 forum thread about UIN registration errors shows ICQ sign-ups still happening ([Escargot forums](https://escargot.chat/forums/110002000201832/icq-uin-help/)). There has been no news post since September 2025 ([nina.chat/news](https://nina.chat/news/)), the NINA forums redirect to Escargot's ([nina.chat/forum](https://nina.chat/forum/)), and there is no status page. The OSCAR service has stayed up continuously since 2023, but the community channels around it are thin.

### The endpoints a client needs

| Host | Port | Purpose per NINA | Role in this client |
|---|---|---|---|
| `login.oscar.nina.chat` | 5190 | Plain OSCAR (FLAP/BUCP), "Standard settings (most clients)" for ICQ ([connect/icq](https://nina.chat/connect/icq/)) | Default login host |
| `slogin.oscar.nina.chat` | 5190 | SSL/TLS OSCAR for "clients that support it" ([NINA news](https://nina.chat/news/120500000101260/some-big-updates-and-fixes-including-new-patcher-audio-chat-and-more/)) | Preferred once verified |
| `kdc.uas.nina.chat` | 443 | UAS/Kerberos login for AIM 6.2+ ([wiki OSCAR](https://wiki.nina.chat/wiki/Protocols/OSCAR)) | Not needed |
| `api.screenname.nina.chat` | 443 | HTTP clientLogin for OSCAR or WIM ([wiki OSCAR](https://wiki.nina.chat/wiki/Protocols/OSCAR)) | Optional alternative login |
| `api.nina.chat` / `api.login.nina.chat` | not stated | WebAPI/WIM, and ICQ 7's login and BOS redirect hosts ([connect/icq](https://nina.chat/connect/icq/)) | Possible future HTTP transport |
| `toc.oscar.nina.chat` | 9898 | TOC/TOC2 ([connect/icq](https://nina.chat/connect/icq/)) | AIM fallback only; whether UINs can sign on is unconfirmed |

The NINA wiki documents four login methods: FLAP channel-1 with a "roasted" password, BUCP/MD5 ("used from AIM 3.5 to AIM 5.9 and for ICQ", and "can be wrapped in TLS"), UAS/Kerberos, and web clientLogin ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)). The authorizer acts as a load balancer and hands back a BOS server address, so BOS hostnames must never be hard-coded ([wiki OSCAR](https://wiki.nina.chat/wiki/Protocols/OSCAR)). NINA publishes no client allow-list, required identity string or minimum version.

NINA explicitly supports several simultaneous logins. IMs and buddy notifications go to every instance, provided the client sends multiconnection TLV 0x4A = 0x01 at BOS sign-on, which the wiki says "should almost always be used" ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)). That lets the phone coexist with a desktop ICQ session.

### Your existing ICQ number must be registered on NINA

Registration currently runs through **account.escargot.chat/register/**, a temporary arrangement while NINA is overhauled ([nina.chat/register](https://nina.chat/register/)). Step one asks for an account type, **Screenname, UIN or PPN**, which "cannot be changed later", plus a date of birth for COPPA. The UIN hint reads "These are numbers, above 10000". UIN accounts "can sign on to MSN and most AIM versions, but not AOL" ([account.escargot.chat/register](https://account.escargot.chat/register/)).

The number is typed in by the user: a July 2026 registrant got "The UIN you entered is invalid" after entering non-digits ([Escargot forums](https://escargot.chat/forums/110002000201832/icq-uin-help/)). NINA users publicly list 5- and 6-digit UINs ([MessengerGeek UIN thread](https://wink.messengergeek.com/t/add-your-icqs-uin-here-nina-and-escargot/25336)). Original AOL accounts were never carried over because AOL would not cooperate ([VOGONS](https://www.vogons.org/viewtopic.php?p=1155521)).

The practical reading for you is simple. If your number is already a NINA account, it works as is. If it is an original 1990s ICQ number, you can probably claim it by typing it in, as long as nobody on NINA has taken it first, although no NINA page states that policy outright. Each email address can hold five accounts ([wiki Extended FAQ](https://wiki.nina.chat/wiki/Support/Extended_FAQ)), and passwords can only be reset, not recovered. NINA also keeps a separate **"legacy password … for use on older versions of MSN, AIM, AOL, etc which lack modern encryption standards"** ([nina.chat FAQ](https://nina.chat/support/faq/)). OSCAR's MD5 and roasted logins are exactly that kind of weak scheme, so the client's login screen should explain the legacy password and let the user enter it.

### What works for ICQ today

The current connect page calls ICQ "beta" and says extended statuses, moods and buddy icons are "still in the works" ([connect/icq](https://nina.chat/connect/icq/)). The news feed fills in the rest. The table puts each feature next to the stance a new client should take.

| Feature | NINA's latest statement | Client stance |
|---|---|---|
| Plain-text IM | "Plain text messaging was improved", July 2025 ([news](https://nina.chat/news/120500000101270/icq-now-in-open-alpha-skype-news-and-other-stuff/)) | MVP |
| Unicode | Cyrillic corruption fixed, roughly Aug–Sept 2025 ([news](https://nina.chat/news/120500000101280/aol-and-icq-fixes-plus-chat-returns-to-aim-8-and-mobile/)) | MVP; test emoji outside the BMP |
| ICQ Server Relay (channel 2) | Supported since Sept 2024 ([news](https://nina.chat/news/120500000101220/this-week-in-updates-2024-09-06/)) | MVP (receive and ack) |
| Statuses and status messages | Older clients' statuses show (early 2025); status-message handling fixed (Sept 2024) ([news](https://nina.chat/news/120500000101250/updates-and-fixes-including-new-patcher-and-file-transfers/), [news](https://nina.chat/news/120500000101220/this-week-in-updates-2024-09-06/)) | MVP |
| Blocking / permit-deny | Block bug fixed; group permit mask added, Sept 2024 ([news](https://nina.chat/news/120500000101220/this-week-in-updates-2024-09-06/)) | Phase 2 |
| Contact list and authorization | "Work is underway on contact lists and authorizations" (early 2025); "More changes to buddies and permissions" (Aug–Sept 2025) ([news](https://nina.chat/news/120500000101260/some-big-updates-and-fixes-including-new-patcher-audio-chat-and-more/), [news](https://nina.chat/news/120500000101280/aol-and-icq-fixes-plus-chat-returns-to-aim-8-and-mobile/)) | Read in MVP; write carefully in Phase 2 |
| Offline messages | "Did not make it into this update", late 2024; nothing since ([news](https://nina.chat/news/120500000101230/aim-60-to-75-now-available-and-other-updates/)) | Probe at runtime |
| Directory / search | "Miscellaneous fixes to unified directory", gender flip fixed ([news](https://nina.chat/news/120500000101230/aim-60-to-75-now-available-and-other-updates/)) | Phase 2 |
| Avatars | Conflict: "Large buddy icons: Supported on applicable AIM and ICQ clients" ([news](https://nina.chat/news/120500000101260/some-big-updates-and-fixes-including-new-patcher-audio-chat-and-more/)) vs "not yet supported" ([connect/icq](https://nina.chat/connect/icq/)) | Phase 3, probe |
| Xtraz / moods | Not supported; NINA "has acquired old ICQ Xtraz and plans to add them" ([news](https://nina.chat/news/120500000101260/some-big-updates-and-fixes-including-new-patcher-audio-chat-and-more/)) | Defer |
| File transfer | Needs router port forwarding; an ICQ proxy is "coming soon" ([news](https://nina.chat/news/120500000101250/updates-and-fixes-including-new-patcher-and-file-transfers/)) | Defer; receive-only at most |
| Typing, SMS, ICQ chat | Never mentioned | Probe |

NINA has also added a UUID (TLV 0x24) to every IM and chat message, intended for offline storage and replies ([NINA news](https://nina.chat/news/120500000101260/some-big-updates-and-fixes-including-new-patcher-audio-chat-and-more/)). That gives the client a natural key for de-duplicating history across sessions.

### The terms permit a human-operated client

The Terms (revision 8 January 2022) do not ban third-party clients. They do forbid "automated scripts to collect, archive, or otherwise interact with NINA", harvesting users' contact data, spam, and anything that could "overburden" the service, and they limit each person to five accounts ([nina.chat/legal/terms](https://nina.chat/legal/terms/)). NINA also promotes its Chat WebAPI "for custom clients or bots" ([NINA news](https://nina.chat/news/120500000101270/icq-now-in-open-alpha-skype-news-and-other-stuff/)).

A normal interactive client is clearly within bounds. Aggressive reconnect loops or bulk directory scraping are the behaviours that would put an account at risk. NINA says it is unaffiliated with AOL and Bending Spoons ([nina.chat](https://nina.chat)), so the app's name and branding should not imply affiliation with ICQ, AOL or NINA either.

## OSCAR in enough depth to implement an ICQ client

The authoritative wire details below come from AOL's 2008 OSCAR specification (mirrored at devinsmith.net), Open OSCAR Server's (OOS) Go `wire` package, Kopete's liboscar, libpurple and Wireshark's dissector. All of them agree on the essentials.

### Every byte travels in a FLAP frame

OSCAR runs over TCP. Every message is a **6-byte FLAP header**: start marker `0x2A`, a u8 frame type, a u16 sequence number and a u16 payload length that excludes the header ([AOL spec mirror](https://devinsmith.net/backups/OSCAR/); [OOS wire/frames.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/frames.go)). The frame types are 1 SIGNON, 2 DATA (carries SNACs), 3 ERROR, 4 SIGNOFF and 5 KEEP_ALIVE.

Sequence numbers start at a random value and increment per frame in each direction. The server closes the connection if they arrive out of order ([AOL spec mirror](https://devinsmith.net/backups/OSCAR/)). Both sides open with a SIGNON frame whose payload begins with the u32 FLAP version `0x00000001`. libpurple drops the connection if any other version arrives ([libpurple flap_connection.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/flap_connection.c)). The keep-alive is an empty channel-5 frame. Send it only after one minute of idleness and never more than once a minute; **the server never answers it** ([AOL spec mirror](https://devinsmith.net/backups/OSCAR/)).

DATA frames carry a **10-byte SNAC header**: u16 foodgroup, u16 subtype, u16 flags and u32 request ID. Flag 0x8000 means a length-prefixed TLV block follows the header. Flag 0x0001 means more replies with the same request ID are coming. Clients must never use request ID 0 or set its high bit; server-initiated SNACs set that bit ([AOL spec mirror](https://devinsmith.net/backups/OSCAR/); [OOS wire/frames.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/frames.go)).

A TLV is u16 tag, u16 length, value. TLVs come in three groupings: a *tlvBlock* prefixed by a u16 count, a *tlvLBlock* prefixed by a u16 byte length, and a *rest block* that runs to the end of the SNAC. Parsers must ignore unknown tags ([OOS wire/tlv.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/tlv.go)). Everything is big-endian except the ICQ-specific payloads (family 0x15 meta data, ICBM channel-4 bodies and channel-2 ICQ headers), which are **little-endian** ([Kopete icqtask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/icqtask.cpp)). Every foodgroup uses subtype 0x0001 for errors, carrying a u16 code: 0x04 "not logged on", 0x0E "busted payload", 0x10 "in local permit/deny" and so on ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)).

The foodgroups an ICQ client touches are:

| Family | Name | Used for |
|---|---|---|
| 0x01 | OSERVICE | Service setup, rates, status |
| 0x02 | LOCATE | Capabilities |
| 0x03 | BUDDY | Presence notifications |
| 0x04 | ICBM | Messages |
| 0x09 | PD | Permit/deny |
| 0x10 | BART | Icons |
| 0x13 | FEEDBAG | Server-side contact list (SSI) |
| 0x15 | ICQ | Meta requests |
| 0x17 | BUCP | Authentication |

### Login: BUCP/MD5 first, roasted channel-1 as fallback

The BUCP flow below is what libpurple, Kopete and OOS all implement, and what NINA documents for ICQ:

```
C→S  TCP connect login.oscar.nina.chat:5190   (or slogin.oscar.nina.chat:5190 with TLS)
S↔C  FLAP ch1  00000001                         (version only, no TLVs)
C→S  SNAC 17/06  TLV01=UIN  [TLV4B empty, TLV5A empty]
S→C  SNAC 17/07  u16 len + authKey
C→S  SNAC 17/02  TLV01=UIN
                 TLV25=MD5(authKey ‖ MD5(password) ‖ "AOL Instant Messenger (SM)")
                 TLV4C (empty = "strong" hash)   TLV03 client string
                 TLV16 clientid  TLV17–1A major/minor/point/build  TLV14 distrib
                 TLV0F "en"  TLV0E "us"  TLV4A = 01 (multi-session)
S→C  SNAC 17/03  TLV05 "host[:port]"  TLV06 cookie  TLV8E ssl-state   | or TLV08 error
S→C  FLAP ch4    signoff → client disconnects
C→S  TCP connect BOS host:port
C→S  FLAP ch1  00000001  TLV06=cookie  TLV4A=01
S→C  SNAC 01/03 HostOnline (list of available families)
```

There are two hash variants. The **weak** hash is `MD5(authKey ‖ password ‖ "AOL Instant Messenger (SM)")` and the **strong** hash is `MD5(authKey ‖ MD5(password) ‖ "AOL Instant Messenger (SM)")`, signalled by an empty TLV 0x4C. OOS accepts either ([OOS wire/user.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/user.go); [libpurple family_auth.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_auth.c)). Kopete sends the weak hash, and it sends TLV 0x4A only for AIM, with a comment that "if set, old-style buddy lists will not work… you will need to use SSI" ([Kopete oscarlogintask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/oscarlogintask.cpp)). A fork should therefore send 0x4A = 0x01 for ICQ too, since it will use SSI anyway. Kopete reads TLV 0x05 as `host:port`, TLV 0x06 as the cookie, TLV 0x8D as the SSL certificate name and TLV 0x8E as the SSL flag.

The older **channel-1 "roasted" login** puts the UIN in TLV 0x01. TLV 0x02 holds the password XORed with the 16-byte table `F3 26 81 C4 39 86 DB 92 71 A3 B9 E6 53 7A 95 7C`, with passwords truncated to 16 characters. The client identity TLVs follow ([OOS wire/user.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/user.go); [libpurple family_auth.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_auth.c)). The server replies with a channel-4 SIGNOFF carrying the BOS address and cookie. NINA's wiki notes that legacy methods were restored for old clients ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)), so implement this as a fallback.

Client identity matters because NINA defines **ICQ-specific "client too old" errors 0x001B and 0x001C** ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)). Kopete's ICQ identity is "ICQ Client", clientid 0x010A, version 6.5.0 build 0x0412, distribution 0 ([Kopete oscarversionupdater.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/oscarversionupdater.cpp)). That is the same ICQ 6.5 that NINA recommends to Windows users ([connect/icq](https://nina.chat/connect/icq/)), which makes it the natural first identity to try. libpurple's alternative is clientid 0x010A, version 20.52.0 build 0x0C18, distribution 0x611 ([libpurple oscar.h](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/oscar.h)).

Login errors arrive in TLV 0x08:

| Code | Meaning |
|---|---|
| 0x0001 | Bad username or password |
| 0x0005 | Bad password |
| 0x0011 | Suspended |
| 0x0018 / 0x001D | Rate limited |
| 0x0016 / 0x0017 | Too many users from this IP |

Sources: [OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go); [NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On).

### Bringing up the BOS session

Once on BOS, the client must wait for **01/03 HostOnline**. It must not send SNACs to families that HostOnline does not list ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)). The wiki says the order of service setup "is not strict", so a robust client tracks the outstanding replies rather than hard-coding a sequence.

| Step | Client → server | Server → client | Purpose |
|---|---|---|---|
| 1 | 01/17 family versions | 01/18 (+ 01/13 MOTD on OOS) | Version negotiation |
| 2 | 01/06 | 01/07 rate classes | Rate parameters |
| 3 | 01/08 class IDs | — | Subscribe to rate changes |
| 4 | 02/02, 03/02, 04/04, 09/02, 13/02 | 02/03, 03/03, 04/05, 09/03, 13/03 | Rights / limits |
| 5 | 02/04 TLV 0x05 caps; 04/02 ICBM params | — | Publish capabilities, enable typing/offline flags |
| 6 | 13/05 (if-modified) or 13/04 | 13/06 list or 13/0F not-modified | Fetch server contact list |
| 7 | 13/07 Use | — | Activate feedbag |
| 8 | 01/1E status + DC info | — | Set ICQ status |
| 9 | 01/02 Client Online | — | Become visible; then request ICQ offline messages |

Sources: [NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On); [OOS foodgroup/oservice.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/oservice.go).

The 01/02 body is a list of quads: family, version, tool ID and tool version. libpurple sends version 0x0003 for OSERVICE, 0x0004 for FEEDBAG and 0x0001 for ICQ 0x15 ([libpurple family_oservice.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_oservice.c)). OOS's BOS HostOnline lists ICQ (0x15) and BART (0x10) directly on BOS ([OOS foodgroup/oservice.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/oservice.go)). NINA's list is unknown and is the first thing to log.

Secondary services such as BART, ChatNav and Chat are requested with **01/04 → 01/05**, which returns a host (TLV 0x05), a cookie (TLV 0x06) and an SSL state (TLV 0x8E). Each secondary service gets its own FLAP connection.

### Rate limits must be modelled on the client

Message 01/07 lists rate classes. Each carries a window size and clear, alert, limit, disconnect, current and max levels, followed by the (family, subtype) pairs mapped to it ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). For each class, the client keeps a moving average of the milliseconds between sends:

```
avg = (avg × (window − 1) + delta) / window
```

It holds outgoing SNACs while the average sits below the alert or limit level. The class is limited until the average climbs back to the clear level, and dropping below the disconnect level ends the session ([OOS wire/rate_limit.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/rate_limit.go)). Kopete already implements this as per-class queues drained by a timer ([Kopete rateclass.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/rateclass.cpp)).

NINA does not publish its parameters, says they change, and says a client can average "around one IM every two seconds" without being limited ([NINA wiki Rate Limits](https://wiki.nina.chat/wiki/Protocols/OSCAR/Rate_Limits)). Read them from 01/07 at every login, and update them on each 01/0A notice (codes 1 change, 2 warning, 3 limit, 4 clear).

### Presence is a 32-bit status word plus capability GUIDs

ICQ status is a u32 set through 01/1E TLV 0x06. The low word holds the state and the high word holds flags ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go); [Kopete oscartypes.h](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/oscartypes.h)).

| Low word | State | Value libpurple actually sends |
|---|---|---|
| 0x0000 | Online | 0x0000 |
| 0x0001 | Away | 0x0001 |
| 0x0004 | N/A | 0x0005 (OUT\|AWAY) |
| 0x0010 | Occupied | 0x0011 (AWAY\|BUSY) |
| 0x0002 | DND | 0x0013 (AWAY\|DND\|BUSY) |
| 0x0020 | Free for chat | 0x0020 |
| 0x0100 | Invisible | 0x0100 |

The high-word flags include 0x00010000 web-aware, 0x00020000 hide-IP and 0x10000000 "direct connection requires auth". libpurple always ORs in hide-IP and DC-auth ([libpurple oscar.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/oscar.c)).

TLV 0x0C carries 37 bytes of direct-connection info. A mobile client should copy libpurple and advertise IP 0, port 0, DC type 0 ("We don't support DC") and protocol version 8 ([libpurple family_oservice.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_oservice.c)). The ICQ 6/AIM-style **status note** travels in TLV 0x1D as a BART item of type 0x0002, capped at **251 bytes** ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)).

Contacts' presence arrives as **03/0B (arrived/changed)** and **03/0C (departed)**. Each carries a user-info block: the screen name, a warning level, and TLVs for user class (0x01, where 0x0040 means ICQ and 0x0020 means away), sign-on time (0x03), idle minutes (0x04), status (0x06), capabilities (0x0D) and BART IDs (0x1D) ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). Capabilities are 16-byte GUIDs published with 02/04 TLV 0x05. The ones that matter first are:

| GUID | Meaning |
|---|---|
| `0946134E-4C7F-11D1-8222-444553540000` | Unicode messages |
| `09461349-4C7F-11D1-8222-444553540000` | ICQ Server Relay (channel 2) |
| `563FC809-0B6F-41BD-9F79-422609DFA2F3` | Typing notifications |
| `0946134D-4C7F-11D1-8222-444553540000` | AIM/ICQ interoperate |
| `09461346-4C7F-11D1-8222-444553540000` | Buddy icon |
| `09461343-4C7F-11D1-8222-444553540000` | Send file |

Source: [libpurple family_locate.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_locate.c).

Advertise only what the client implements. Classic ICQ away messages are not stored on the server. A client fetches them with a channel-2 "auto message" request (types 0xE8 away through 0xEC free-for-chat), and the answer comes back as 04/0B ([Kopete sendmessagetask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/sendmessagetask.cpp); [libpurple family_icbm.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_icbm.c)).

### The contact list lives on the server as feedbag items

The SSI ("feedbag") list is a flat set of items. Each one has a name (u16 length plus bytes), a group ID, an item ID, a class ID, and a length-prefixed TLV block of attributes ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). The classes that matter are:

| Class | Item |
|---|---|
| 0x0000 | Buddy |
| 0x0001 | Group |
| 0x0002 | Permit (visible list) |
| 0x0003 | Deny (invisible list) |
| 0x0004 | Privacy settings |
| 0x0005 | Buddy preferences |
| 0x000E | Ignore |
| 0x0014 | Buddy icon |

The attributes that matter are 0x0066 (authorization pending), **0x00C8 order lists**, 0x0131 (alias), 0x013C (note) and 0x00CA (privacy mode: 1 permit all, 2 deny all, 3 permit some, 4 deny some, 5 permit buddy list). In the order lists, each group lists its buddy IDs, and the root group 0 lists the group IDs. Official clients render groups from these lists, so every edit must keep them consistent.

The client fetches the list with 13/05, which returns 13/06 or "not modified" 13/0F, and then activates it with 13/07. Edits go inside a 13/11 … 13/12 transaction, using 13/08 insert, 13/09 update and 13/0A delete. Each operation returns a 13/0E result code:

| Code | Meaning |
|---|---|
| 0 | OK |
| 2 | Not found |
| 3 | Exists |
| 0x0E | Authorization required |
| 0x11 | Over buddy limit |

Sources: [Wireshark packet-aim.c](https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-aim.c); [Kopete ssimodifytask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/ssimodifytask.cpp).

NINA's wiki warns that "a client developer must be careful not to corrupt things" ([wiki OSCAR](https://wiki.nina.chat/wiki/Protocols/OSCAR)), so write support belongs in Phase 2, behind tests.

ICQ authorization uses its own SNACs:

| Step | SNACs |
|---|---|
| Request | 13/18 to host → 13/19 delivered to the recipient |
| Reply | 13/1A to host → 13/1B delivered to the requester |
| Notice | 13/1C "you were added" |
| Pre-authorization | 13/14 / 13/15 |

On OOS, adding an ICQ contact who requires authorization fails with 0x0E. The client must retry with attribute 0x0066 set. OOS also bridges these SNACs to channel-4 auth messages for clients that do not use feedbag ([OOS foodgroup/feedbag.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/feedbag.go)). The robust add-contact flow therefore opens a transaction with 13/11 and inserts the buddy with 13/08. On a 0x0E result it inserts the buddy again with 0x0066 set and sends an authorization request with 13/18. It then updates the parent group's 0x00C8 order list with 13/09 and closes the transaction with 13/12. When 13/1B arrives with "accepted", the client clears 0x0066.

### Messages travel on three ICBM channels

An outgoing IM is **04/06**: an 8-byte cookie, a u16 channel, a u8-length recipient, then TLVs. Incoming IMs are 04/07, with the sender's user-info block in place of the recipient ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). The channels are used as follows.

**Channel 1 (normal IM).** The text goes in TLV 0x02 as a series of fragments, each with a u8 ID, u8 version, u16 length and payload. Fragment 0x05 lists features; Kopete sends `01 06` for ICQ. Fragment 0x01 holds the message: a u16 charset, a u16 subset, then the text. The charsets are 0x0000 ASCII, 0x0002 UTF-16BE and 0x0003 ISO-8859-1 ([libpurple oscar.h](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/oscar.h); [Kopete sendmessagetask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/sendmessagetask.cpp)).

The other 04/06 TLVs control delivery. TLV 0x03 requests a server ack (04/0C), and TLV 0x06 asks the server to store the message if the recipient is offline. Without 0x06, OOS returns error 0x0004 for an offline recipient ([OOS foodgroup/icbm.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/icbm.go)). Kopete warns that TLVs 0x03 and 0x04 cannot appear in the same SNAC ([Kopete sendmessagetask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/sendmessagetask.cpp)). libpurple caps outgoing messages at 2,544 bytes ([libpurple oscar.h](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/oscar.h)).

**Channel 2 ("type-2" / Server Relay).** The ICQ message sits inside the rendezvous TLV 0x05: type u16, the cookie, capability GUID 09461349…, then TLV 0x2711. TLV 0x2711 holds a little-endian ICQ header ([Kopete sendmessagetask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/sendmessagetask.cpp)):

```
u16 0x001B | u16 protoVer (8–10) | 16×00 plugin GUID | u16 0 | u32 features=3 | u8 0
u16 counter | u16 0x000E | u16 counter | 12×00
u8 msgType | u8 flags | u16 status | u16 priority
u16 len(text+NUL) | text | NUL | u32 fg=0 | u32 bg=0x00FFFFFF
[u32 38 | "{0946134E-4C7F-11D1-8222-444553540000}"]   ← present ⇒ text is UTF-8
```

The receiver must confirm with **04/0B**, reason 0x0003, echoing the header with empty text. libpurple notes that "Miranda wants it by default" ([libpurple family_icbm.c](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/family_icbm.c)).

**Channel 4 (legacy ICQ).** The body is a little-endian UIN, a u8 type, a u8 flags byte, then length-prefixed text. The types are:

| Type | Meaning |
|---|---|
| 0x01 | Plain |
| 0x04 | URL |
| 0x06 | Auth request |
| 0x07 | Auth denied |
| 0x08 | Auth granted |
| 0x0C | You were added |
| 0x13 | Contacts |
| 0xE8–0xEC | Auto-messages |

Source: [OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go).

**Typing notifications** are 04/14: a zero cookie, channel 1, the screen name, and an event of 0 finished, 1 paused or 2 typing ([Wireshark packet-aim.c](https://github.com/wireshark/wireshark/blob/master/epan/dissectors/packet-aim.c)). OOS relays typing only when the sender's feedbag buddy-preferences item has preference bit 0x16 set ([OOS foodgroup/icbm.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/icbm.go)). NINA may impose the same rule.

That gives a sensible default for a modern client. Send on channel 1, using UTF-16BE whenever the text is not pure ASCII, with TLVs 0x03 and 0x06. Accept all three channels and all three charsets on receive. Honour the trailing UTF-8 GUID on channel 2, and decode channel 4 as Latin-1 or a configurable codepage.

### Family 0x15 carries ICQ's offline messages, user info and search

Every ICQ meta request is **15/02** with a single TLV 0x0001. Its value is little-endian: a u16 chunk length, the u32 UIN, a u16 request type, a u16 sequence number, and, for type 0x07D0, a u16 subtype plus data. Replies arrive as 15/03 with the same envelope. Type 0x07DA replies carry a subtype and a status byte (0x0A OK, 0x14 error, 0x32 failure) ([Kopete icqtask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/icqtask.cpp); [OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)).

Strings inside meta payloads are "ICQ strings": a u16 LE length that counts the trailing NUL, then the bytes, then the NUL ([OOS wire/tlv.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/tlv.go)).

**Offline messages.** The client sends request **0x003C** after 01/02. The server returns one 0x0041 record per message, flagged "more to come". Each record holds the sender UIN, year, month, day, hour, minute, type, flags and text. The server then sends 0x0042 "end", and the client acknowledges with 0x003E so the server deletes them ([OOS foodgroup/icq.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/icq.go)). AIM-style accounts use 04/10 instead, which replays stored 04/07 messages ([OOS foodgroup/icbm.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/icbm.go)). Because NINA's status here is unknown, the client should try both and record which one answers.

**User info and search.** Meta subtypes cover the rest:

| Subtype | Request | Notes |
|---|---|---|
| 0x04BA | Short info | Reply 0x0104: nick, first, last, email, auth flag, gender |
| 0x04B2 / 0x04D0 | Full info | Answered as a series of basic, more, email, work, notes and interests replies |
| 0x051F | Search by UIN | Hits as 0x01A4 records, the last as 0x01AE with a users-left count |
| 0x0529 | Search by email | Same reply format |
| 0x0515 | Search by name | Same reply format |
| 0x055F | Whitepages (TLV form) | Same reply format |
| 0x03EA | Set basic info | |
| 0x0424 | Set permissions | Auth-required, web-aware, DC permissions |

Source: [OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go).

Kopete implements password change as subtype 0x042E, expecting reply 0x00AA ([Kopete icqchangepasswordtask.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/tasks/icqchangepasswordtask.cpp)). OOS does not support it. Since NINA manages passwords on the web, the app should link out rather than rely on it.

NINA's own 0x15 wiki page is a stub ([wiki SNAC_15](https://wiki.nina.chat/wiki/Protocols/OSCAR/SNAC_15)). OOS pads some replies to keep ICQ 5 happy, so parsers must tolerate trailing bytes.

### Icons, files and chat rooms are optional extras on a phone

**Buddy icons.** To publish an icon, the client stores a feedbag item of class 0x0014 named "1", whose TLV 0x00D5 holds the icon's MD5. If the server flags the hash as unknown (0x40), the client uploads the image with 10/02. Contacts' icons appear in user-info TLV 0x1D and are fetched with 10/06 → 10/07 ([OOS foodgroup/feedbag.go](https://github.com/mk6i/retro-aim-server/blob/main/foodgroup/feedbag.go); [OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). libpurple caps icons at 7,168 bytes ([libpurple oscar.h](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/oscar.h)).

**File transfer.** A transfer is a channel-2 proposal with the SendFile capability, IP and port TLVs, and file info in TLV 0x2711. The peers then connect directly, or through an ARS proxy, and exchange 256-byte "OFT2" headers ([libpurple peer.h](https://github.com/tieto/pidgin/blob/release-2.x.y/libpurple/protocols/oscar/peer.h)). NINA requires port forwarding for this ([NINA news](https://nina.chat/news/120500000101250/updates-and-fixes-including-new-patcher-and-file-transfers/)), which is unrealistic behind mobile carrier NAT, so defer it.

**Chat rooms.** AIM chat uses ChatNav (0x0D) and Chat (0x0E) on separate connections obtained through 01/04. Messages go out as 0E/05 and arrive as 0E/06 ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). This is an AIM feature rather than an ICQ one, so it belongs in the AIM phase.

### Pitfalls that break naive implementations

Screen names are compared lowercased with spaces removed, and a name made entirely of digits is a UIN ([OOS state/user.go](https://github.com/mk6i/retro-aim-server/blob/main/state/user.go)). Endianness switches mid-packet in ICQ structures. A u8 sequence counter would roll over and get the connection dropped ([AOL spec mirror](https://devinsmith.net/backups/OSCAR/)). OOS aborts authentication after 30 seconds or more than 10 frames ([OOS server/oscar/server.go](https://github.com/mk6i/retro-aim-server/blob/main/server/oscar/server.go)). OOS's UCS-2 path replaces characters above U+FFFF with U+FFFD, so emoji can be lost ([OOS wire/snacs.go](https://github.com/mk6i/retro-aim-server/blob/main/wire/snacs.go)). Reconnects need exponential backoff with jitter, and a fast retry after a rate-limit error (0x18 or 0x1D) invites a longer lockout.

The best reading order for the implementer starts with the AOL spec mirror for framing, then OOS `wire/snacs.go` as the numeric table of a working server, then Kopete liboscar for a Qt task-per-exchange design, then libpurple for ICQ edge cases, and finally Shutko's iserverd OSCAR documents via the Wayback Machine ([Wikipedia: OSCAR](https://en.wikipedia.org/wiki/OSCAR_protocol)).

## Kopete's liboscar is the engine; retro-prpl is the reference

### Five protocol engines compared

| Codebase | Stack | Licence | Last relevant activity | ICQ coverage | Fit for Sailfish |
|---|---|---|---|---|---|
| Kopete liboscar | C++/Qt5, light KDE | LGPL-2.0+ | OSCAR code Nov 2023 | SSI, auth, offline, typing, icons, info/search, chat, files | **Best**: declares Qt 5.6.0 minimum |
| retro-prpl `oscar/` | C/GLib (libpurple) | GPL-2 | Aug 2026 | Everything, 27k lines | Reference; engine only with libpurple ≥2.14 |
| Sailfish libpurple 2.10.x + telepathy-haze | C/GLib, D-Bus | GPLv2+ | 2016 / 2020 | Stock oscar prpl | Messages-app integration only |
| qutIM oscar plugin | C++/Qt5, qbs | GPL-3.0+ | July 2015 | Richest ICQ (xtraz, meta, identify) | Heavy framework coupling |
| Licq icq plugin | C++, no Qt | GPL-2 | Oct 2014 | Classic ICQ | Stale |

Sources: [KDE/kopete](https://github.com/KDE/kopete); [pidgin/retro-prpl](https://github.com/pidgin/retro-prpl); [sailfishos/libpurple](https://github.com/sailfishos/libpurple); [sailfishos/telepathy-haze](https://github.com/sailfishos/telepathy-haze); [euroelessar/qutim](https://github.com/euroelessar/qutim); [licq-im/licq](https://github.com/licq-im/licq).

### Why Kopete liboscar wins

Kopete's top-level build sets **`REQUIRED_QT_VERSION "5.6.0"`**, exactly Sailfish's Qt version ([Kopete CMakeLists.txt](https://github.com/KDE/kopete/blob/master/CMakeLists.txt)). The `protocols/oscar/liboscar` directory holds 163 files, about 805 KB. The files read so far carry an LGPL-2.0-or-later header ([Kopete client.cpp](https://github.com/KDE/kopete/blob/master/protocols/oscar/liboscar/client.cpp)).

Its design is already the right one: one Task class per SNAC exchange. There are tasks for login, SSI listing, editing, authorization and activation, offline messages, typing, buddy icons, file transfer, chat navigation and rooms, user search, ICQ info requests and updates, password change, and message sending, receiving and acks. Supporting classes cover FLAP/SNAC protocol handling, rate-class management and a contact manager ([Kopete liboscar](https://github.com/KDE/kopete/tree/master/protocols/oscar/liboscar)).

The KDE coupling inside the library is small. It amounts to 64 `kdebug.h` uses, 10 of `KLocalizedString`, 5 of `krandom.h`, 2 of `ksocketfactory.h` and 1 of `kdemacros.h`, each with a mechanical Qt replacement: QLoggingCategory, `tr()`, `qrand()` and QTcpSocket/QSslSocket.

The doubts are age and server tuning, not structure. Kopete was dropped from KDE Gear 24.02 ([KDE Gear 24.02 notes](https://community.kde.org/KDE_Gear/24.02_Release_notes)). The 2023 removal proposal noted that it "silently fails to connect to ICQ" because Mail.ru's ICQ changed protocols ([kde-devel](https://mail.kde.org/pipermail/kde-devel/2023-July/001908.html)), which does not matter against NINA's classic protocol. A community member did get Kopete's AIM plugin working against NINA ([VOGONS](https://www.vogons.org/viewtopic.php?p=1155521)). My estimate for extracting, de-KDE'ing and building it with qmake for armv7hl is one to three weeks for one experienced Qt developer, not counting the UI.

### retro-prpl, libpurple and the Telepathy shortcut

Pidgin removed the AIM plugin from stock Pidgin 2 in version 2.14.3. The Pidgin team now maintains **retro-prpl**, which restores OSCAR (both AIM and ICQ), TOC, MSN and other protocols for libpurple ≥ 2.14.0. Its README names "Open OSCAR Server, escargot, and NINA" as the networks that motivated it. Its last commit was 29 August 2026, and its `oscar/` directory is 27,114 lines of C ([pidgin/retro-prpl](https://github.com/pidgin/retro-prpl)). That makes it the code most likely to track NINA's behaviour over time. When liboscar and NINA disagree, check what retro-prpl does.

Using retro-prpl as the app's engine is a different matter. Sailfish's own libpurple is 2.10.11/2.10.12 from 2016 ([sailfishos/libpurple](https://github.com/sailfishos/libpurple)). The app would have to bundle a newer GLib-based libpurple and write C-to-Qt glue for libpurple's UI callbacks, and the whole app would become GPL-2-compatible.

The Telepathy route is the lowest-code way to get ICQ into Sailfish's own Messages app. Sailfish's commhistory-daemon logs any Telepathy text channel, whichever connection manager provides it ([commhistory-daemon logger.cpp](https://github.com/sailfishos/commhistory-daemon/blob/master/src/logger.cpp)). CODeRUS's 2015 "ICQ Accounts" plugin wired libpurple's ICQ protocol through telepathy-haze into Settings → Accounts, with configurable server and port ([haze-accounts-extensions](https://github.com/CODeRUS/haze-accounts-extensions)). The packaged version from 2017 was marked "ONLY FOR TESTING", and users found it worked on one device and failed on others ([OpenRepos ICQ Accounts](https://openrepos.net/node/10019)).

The approach needs system-wide files that Harbour forbids, it limits the user experience to whatever Messages exposes, and no example has been confirmed working on Sailfish OS 4.5/5.x. It is a possible Phase 4 experiment, not the foundation.

### qutIM, Licq and the dead ends

qutIM's OSCAR plugin has the richest ICQ feature set, including xtraz, meta search, client identification and privacy lists. It is also GPL-3.0-or-later, builds with qbs, includes 31 distinct libqutim headers, uses QtWidgets settings pages, and the project was declared end-of-life in 2023 ([euroelessar/qutim](https://github.com/euroelessar/qutim)). Its MeeGo Harmattan QML account pages are worth reading for ideas, but porting the plugin costs more than it returns.

The other candidates fit worse. **Licq**'s ICQ plugin is Qt-free C++ under GPL-2, untouched since 2014 ([licq-im/licq](https://github.com/licq-im/licq)). **Miranda NG**'s ICQ plugin speaks Mail.ru's WIM rather than OSCAR and is no longer developed ([Miranda NG wiki](https://wiki.miranda-ng.org/index.php?title=Plugin%3AICQ)). **BitlBee** deleted its native OSCAR code in 2019 ([bitlbee](https://github.com/bitlbee/bitlbee)). None of them is worth adapting.

**Open OSCAR Server** (MIT, Go, committed to on 18 October 2026) supports ICQ messaging, profiles, search, presence and offline messages for ICQ 98 through 5 ([mk6i/retro-aim-server](https://github.com/mk6i/retro-aim-server)). Run it in Docker as the CI test server, and use its `wire` package as a byte-level specification. It is not NINA's server, so passing against it is necessary but not sufficient.

### Sailfish apps to borrow the shell from

**Fernschreiber** (Telegram, GPL-3.0-or-later, committed to on 11 October 2026, armv7hl RPMs on OpenRepos) is the structural template ([harbour-fernschreiber](https://github.com/Wunderfitz/harbour-fernschreiber); [OpenRepos](https://openrepos.net/node/11155)). Its C++ backend splits into a protocol wrapper and receiver, a `ChatListModel` and a `ChatModel`, a `NotificationManager`, an MCE interface and a D-Bus adaptor. QML pages for the chat list, conversation, information, search, settings and cover sit on top. Swapping TDLib for an OSCAR client object keeps everything else.

Three other apps fill gaps. **Whisperfish** (Signal, AGPL-3.0) has the jolla-messages look and the opt-in systemd background service pattern ([whisperfish](https://github.com/whisperfish/whisperfish)). **Shmoose** (XMPP) is the only client found that holds a persistent TCP session through suspend, using libiphb heartbeats and reconnect logic, which makes it the closest analogue to OSCAR ([IpHeartBeatWatcher.cpp](https://github.com/geobra/harbour-shmoose/blob/master/source/networkconnection/IpHeartBeatWatcher.cpp)). **Communi** (IRC, LGPL/BSD) shows a minimal Internet-only sandbox ([communi-sailfish](https://github.com/communi/communi-sailfish)).

The Opal component library (mostly GPLv3) supplies an About page and other Silica-style modules ([Opal](https://forum.sailfishos.org/t/opal-qml-components-for-app-development/15801)).

## Build a Qt-only core with a Silica shell, ship on Chum first

### Platform constraints that shape the design

The Xperia XA2 remains a supported 32-bit device on **Sailfish OS 5.0.0.76** (released March 2026) ([release notes](https://forum.sailfishos.org/t/release-notes-tampella-5-0-0-76/28285)), and Sailfish OS 5 "still ships Qt 5.6" ([xmatic README](https://github.com/JimKnopfIoT/harbour-xmatic)). That rules out QtQuick.Controls and anything past QtQuick 2.6, and limits QML JavaScript to ES5, so no `let` or arrow functions ([Harbour Allowed APIs](https://docs.sailfishos.org/Develop/Apps/Harbour/Allowed_APIs/)). Harbour allows libQt5Network, libQt5Sql, libQt5DBus, OpenSSL 3, libsqlite3, libkeepalive and libnemonotifications-qt5 ([Harbour Allowed APIs](https://docs.sailfishos.org/Develop/Apps/Harbour/Allowed_APIs/)). It forbids QtWidgets, and it allows private bundled libraries only under `/usr/share/<app>/lib` with a correct RPATH and no RPM "Provides" ([Harbour FAQ](https://harbour.jolla.com/faq)). A core built on Qt5Core plus Qt5Network fits all of that.

Every app has been sandboxed since Sailfish OS 4.4. Permissions are declared in the `.desktop` file, writable data lives under `~/.local/share/<Org>/<App>`, and a default-constructed QSettings no longer works ([sailjail-permissions README](https://github.com/sailfishos/sailjail-permissions/blob/master/README.md)).

**Background operation is the hard constraint.** OSCAR has no push, and Sailfish offers native apps no push service, so messages arrive only while the process holds its socket. Fernschreiber users report that notifications work only while the app runs ([forum](https://forum.sailfishos.org/t/bump-native-app-background-notifications/11908)). The Harbour-allowed tool is Nemo.KeepAlive's `BackgroundJob` (or C++ `BackgroundActivity`), which wakes the process on IPHB slots such as 30 seconds, 2.5 minutes or 5 minutes, even from late suspend ([nemo-keepalive](https://github.com/sailfishos/nemo-keepalive/blob/master/lib/backgroundactivity.h)).

The Shmoose author reported that from Sailfish OS 3.3 onward, TCP connections dropped during sleep even with iphb wake-ups. The only reliable workaround was disabling late suspend with `mcetool -searly`, at "only slightly higher" battery cost ([forum](https://forum.sailfishos.org/t/device-suspend-and-stable-longterm-tcp-connections/3385)). Whisperfish's background mode is a systemd user unit started after the user session, plus D-Bus activation and an autostart switch. Toggling the switch needs the AppLaunch permission, which Harbour does not allow ([harbour-whisperfish.service](https://github.com/whisperfish/whisperfish/blob/main/harbour-whisperfish.service); [Harbour Allowed Permissions](https://docs.sailfishos.org/Develop/Apps/Harbour/Allowed_Permissions/)).

The design therefore needs four pieces. The first is a wake-up every 2.5–5 minutes that sends a channel-5 keep-alive. The second is a liveness probe: the server never answers keep-alives, so the client sends a cheap request-reply SNAC such as 01/0E self-info and declares the socket dead if 01/0F does not arrive in time. The third is a reconnect on every network change. The fourth is an optional "stay awake while connected" toggle using `KeepAlive.enabled`, for users who accept the battery cost. Measure screen-off survival on the XA2 for 30 minutes or more in the first sprint.

**Notifications** should copy Fernschreiber, whose approach stays Harbour-safe: one notification per conversation in the system category `x-nemo.messaging.im`, a remote D-Bus action that opens the chat through `ExecDBus`, and the MCE LED pattern `PatternCommunicationIM` ([notificationmanager.cpp](https://github.com/Wunderfitz/harbour-fernschreiber/blob/master/src/notificationmanager.cpp)). Inline reply actions have existed since October 2021 but only work reliably while the process runs ([nemo-qml-plugin-notifications](https://github.com/sailfishos/nemo-qml-plugin-notifications/blob/master/src/notification.cpp)).

### Architecture

```
┌──────────────── QML / Silica UI ───────────────────────────────┐
│ ContactListPage · ConversationPage · ContactInfoPage            │
│ SearchPage · AccountPage(login, server, legacy pw) · Settings   │
│ CoverPage (unread totals, status action)                        │
└──────────────▲──────────────────────────────────────────────────┘
               │ QAbstractListModels, Q_PROPERTY, signals
┌──────────────┴──────────── App core (C++) ───────────────────────┐
│ AccountManager · ContactListModel · ConversationModel            │
│ HistoryStore (QtSql/SQLite, dedupe on NINA TLV 0x24 UUID)        │
│ NotificationManager (x-nemo.messaging.im, D-Bus actions, LED)    │
│ CredentialStore (Sailfish Secrets) · ConnectionKeeper            │
│ (BackgroundActivity, liveness probe, backoff, network watcher)   │
│ FeatureProbe (HostOnline families, rate classes, 0x15 replies)   │
│ ServerProfiles (NINA plain, NINA TLS, local OOS, custom)         │
│ ProtocolTrace (opt-in hex log of FLAP/SNAC for bug reports)      │
└──────────────▲───────────────────────────────────────────────────┘
               │ Client API: login(), sendMessage(), setStatus(), …
┌──────────────┴──── liboscar fork (LGPL-2.0+, private .so) ───────┐
│ Client · FLAP Connection · RateClassManager · ContactManager(SSI)│
│ Tasks: login(BUCP/roast) · ssi* · sendmessage · messagereceiver │
│        typingnotify · offlinemessages · icquserinfo · usersearch │
│        buddyicon · chatnav/chat · …                              │
│ Depends on Qt5Core + Qt5Network only                             │
└──────────────────────────────────────────────────────────────────┘
```

Everything can run on the single Qt event loop, because OSCAR traffic is light. Keep the library's API free of UI types so that the same `.so` can later back a Telepathy connection manager.

Two components do the most to de-risk development against an undocumented server. `FeatureProbe` turns what NINA actually answers into feature flags that hide unsupported UI. `ProtocolTrace` lets users attach hex traces to bug reports, either for you or for NINA's support form.

### Licensing

Keep the liboscar fork **LGPL-2.0-or-later**, as a separately built shared library. License the app **GPL-3.0-or-later** so it can freely borrow from Fernschreiber and Opal. LGPL code links into GPLv3 without conflict, and bundling the `.so` privately is Harbour-legal.

Take only patterns, not code, from Whisperfish, because copying its AGPL code would make the app AGPL. Treat retro-prpl as a behavioural reference too. If any of its code is copied, check that file's header first, because GPL-2-only code cannot be combined with a GPLv3 app.

Before the first release, audit every liboscar file header. Only `client.cpp` has been checked so far, and some files may descend from other projects.

Avoid "ICQ", "AIM" or "NINA" as the app's name or organization. Use a neutral package name with the `harbour-` prefix, such as `harbour-<yourname>`, and say "for ICQ and AIM accounts on NINA" in the description.

### Distribution

Sailfish:Chum should be the primary channel. It builds reproducible RPMs on OBS for each architecture and Sailfish release, imposes "no major restrictions", and installs through the Chum GUI or `zypper` ([sailfishos-chum/main](https://github.com/sailfishos-chum/main/blob/master/README.md)). Mirror the RPMs on OpenRepos for Storeman users, where Whisperfish, xmatic and Fernschreiber already ship ([OpenRepos messenger tag](https://openrepos.net/tags/messenger)).

Add a Harbour flavour later behind a spec switch, as Whisperfish does with `%{with harbour}` ([Whisperfish spec](https://github.com/whisperfish/whisperfish/blob/main/rpm/harbour-whisperfish.spec)). That flavour drops the systemd unit and AppLaunch and keeps foreground running plus `BackgroundJob`. A Harbour-compatible sandbox declaration looks like this:

```
[X-Sailjail]
Permissions=Internet;Secrets;Pictures;Downloads;MediaIndexing;Audio
OrganizationName=<your.reverse.domain>
ApplicationName=<appname>
ExecDBus=harbour-<appname>
```

Add `Contacts` only if the app links contacts to People. `Secrets` (Sailfish OS 4.5+) keeps the password and legacy password out of plain-text config ([sailjail-permissions README](https://github.com/sailfishos/sailjail-permissions/blob/master/README.md)). Use `X-Nemo-Application-Type=silica-qt5` for booster start-up.

### Feature plan in four phases

| Phase | Scope | Protocol work | Exit criterion |
|---|---|---|---|
| 0 · Spike (1–2 wks) | Fork liboscar, strip KDE, qmake build for armv7hl in the Sailfish SDK; console harness; local OOS in Docker | BUCP weak/strong, roasted fallback, TLS on slogin; log HostOnline, 01/07, login errors | Your UIN online on NINA, one message each way, trace archived |
| 1 · MVP | Single ICQ account, Silica UI, history, notifications, cover, keep-alive and reconnect, Secrets | SSI read + 13/07; 01/1E statuses incl. invisible; ch1 UTF-16 send/receive; ch2 receive + 04/0B ack; ch4 receive; typing 04/14; caps via 02/04 | Survives 30+ min screen-off on the XA2; no rate-limit warnings in normal use |
| 2 · ICQ complete | Contact management, authorization, offline, info/search, privacy, status notes, background mode (Chum build) | 13/11–13/12 edits with 0x00C8 order; 13/18–13/1B and ch4 auth; 0x15 0x003C/0x003E (+04/10 probe); 0x04BA/0x04D0/0x051F/0x0529/0x0515; PD lists and mode; BART 0x0002 note; 0xE8–0xEC away fetch | Parity with ICQ 6.5 for daily use; feature flags hide whatever NINA lacks |
| 3 · AIM and extras | AIM screen-name accounts, multiple accounts, buddy icons, AIM chat rooms, file receive, share-to-app | AIM identity and strong hash; LOCATE profile/away; BART 10/02 and 10/06; ChatNav/Chat; OFT receive | ICQ↔AIM conversation verified on NINA |
| 4 · Optional | Telepathy connection manager reusing the library (Chum-only), Harbour flavour, WIM transport, Xtraz when NINA ships it | TelepathyQt service; `api.nina.chat` WIM | Driven by user demand and NINA's roadmap |

AIM is cheap in Phase 3 because the same OSCAR stack serves both networks. NINA's joint "AIM/ICQ" fixes point to one shared backend ([NINA news](https://nina.chat/news/120500000101280/aol-and-icq-fixes-plus-chat-returns-to-aim-8-and-mobile/)), and Escargot screen names can sign on to AIM directly ([nina.chat/connect/aim](https://nina.chat/connect/aim/)).

## Risks and the questions only the live server can answer

### Risk register

| Risk | Why it is real | Mitigation |
|---|---|---|
| NINA disappears or moves | 2025 Discord deletion and "legal reasons" shutdown; no news since ([NINA news](https://nina.chat/news/120500000101300/what-happens-next/)) | Configurable server profiles; keep OOS compatibility; local history export |
| NINA's ICQ lacks features | Officially beta; offline and auth unconfirmed since late 2024 ([connect/icq](https://nina.chat/connect/icq/)) | FeatureProbe flags; graceful degradation; report traces |
| Client identity rejected | ICQ "client too old" codes 0x001B/0x001C exist ([NINA wiki Sign On](https://wiki.nina.chat/wiki/Protocols/OSCAR/Sign_On)) | Identity profiles (ICQ 6.5, libpurple, ICQ 2003b), switchable in settings |
| Account suspension | An unverified insider claim of 1,000+ accounts suspended in Sept 2025 ([MessengerGeek](https://wink.messengergeek.com/t/nina-escargot-from-the-inside/26336)); ToS bans overburdening ([terms](https://nina.chat/legal/terms/)) | Honour 01/07; exponential backoff; never auto-retry on 0x18/0x1D or 0x11 |
| Socket dies in deep sleep | Reported since Sailfish OS 3.3 ([forum](https://forum.sailfishos.org/t/device-suspend-and-stable-longterm-tcp-connections/3385)) | Wake-ups + liveness probe; optional stay-awake toggle; NINA's multi-session support keeps a desktop session as backstop |
| Kopete code rot | Unmaintained since 2023–24 ([KDE Gear 24.02](https://community.kde.org/KDE_Gear/24.02_Release_notes)) | Own the fork; unit tests against OOS; diff behaviour against retro-prpl |
| Platform shift | Jolla is reportedly weighing a Qt6 Silica ([forum](https://forum.sailfishos.org/t/upgrading-silica-to-qt6/26712/7)); no published XA2 end-of-support date | Keep the protocol core free of Silica; avoid deprecated `org.nemomobile.*` imports |

### Verification checklist against NINA in week one

| # | Question | How to probe | Fallback |
|---|---|---|---|
| 1 | Is your UIN registered, and which password works for OSCAR, the main one or the legacy one? | Log in with each via BUCP | Set a legacy password in the account area |
| 2 | Does NINA accept the strong hash (TLV 0x4C), the weak hash, or roasted channel-1 for UINs? | Try each, log 17/03 TLV 0x08 | Order methods by success |
| 3 | Does `slogin.oscar.nina.chat:5190` speak TLS before FLAP? Which certificate and SNI? Does BOS TLS follow (TLV 0x8E)? | QSslSocket handshake; inspect cert | Plain 5190 |
| 4 | Is the identity "ICQ Client 6.5.0.0x0412" accepted? Does TLV 0x03 matter? | Vary identity TLVs | libpurple identity |
| 5 | What is the BOS TLV 0x05 format and port? | Log 17/03 | Default to 5190 when no port |
| 6 | Which families does 01/03 list? Are 0x15 and 0x10 on BOS? | Log HostOnline | Request via 01/04 |
| 7 | Actual rate classes and ICBM class mapping | Log 01/07 | Conservative 1 IM / 2 s cap |
| 8 | Do ICQ offline messages come via 0x15 0x003C or 04/10, and does 0x003E delete them? | Send while offline from a second account | Show "may not be delivered" hint |
| 9 | Does adding an auth-required UIN return 0x0E, and do 13/18 → 13/1B round-trip? | Two test UINs | Channel-4 auth types 0x06/0x08 |
| 10 | Are typing notifications relayed? Is buddy-prefs bit 0x16 required? | Toggle the feedbag pref | Hide typing indicator |
| 11 | How do UTF-16 channel 1, UTF-8 channel 2 and non-BMP emoji survive to ICQ 6.5 and QIP? | Cross-client tests | Strip or replace emoji |
| 12 | Which 0x15 meta searches and info replies work, and do replies carry padding? | Search own UIN, email, name | Hide search modes |
| 13 | Are BART icons served for UIN accounts? | 10/06 for a known icon | Initials avatars |
| 14 | Does ICQ↔AIM screen-name messaging work, and over which channel? | UIN ↔ screen name test | Label as unsupported |
| 15 | Server idle timeout and keep-alive tolerance with the screen off | Vary wake interval 2.5/5/10 min | Shorter interval or stay-awake |
| 16 | Does multi-session with a desktop ICQ deliver to both, and does history duplicate? | Run ICQ 6.5 alongside | De-duplicate on TLV 0x24 UUID |

## Conclusion

NINA is the risk, not OSCAR. The protocol is a solved problem: AOL's spec, OOS's Go tables, Kopete's Qt tasks and libpurple's C all describe the same bytes. The work lies in conforming empirically to a proprietary beta server whose own documentation is largely historical, and in keeping a socket alive on a phone with no push. The most valuable early code is therefore the instrumentation, a feature probe and a protocol trace, because it turns NINA's moving target into data the UI can react to and evidence NINA's staff can act on. Two choices insure the project against NINA's governance fragility without much extra cost: support Open OSCAR Server as a second target, and put the protocol in its own LGPL library.

The project also fills more than one gap. No maintained native OSCAR client exists for Sailfish, and searches found none for Ubuntu Touch either. NINA's own ICQ guide still points users at Windows clients, config-file edits and hex-patching ([connect/icq](https://nina.chat/connect/icq/)). A clean LGPL, Qt-only `liboscar` that works against NINA would be reusable beyond Sailfish: by a Telepathy connection manager, by an Ubuntu Touch or desktop Qt client, or by the FOSS connectors NINA said in 2024 it was developing ([The Register](https://www.theregister.com/2024/05/31/nina_saving_icq/)). Fixes contributed back to the NINA wiki would help every third-party developer after you. Start with the week-one spike against your own UIN. Its first trace will answer more of the open questions above than any further desk research could.
