// Photos, voice messages and stickers travel as ordinary messages with a
// download link, so every ICQ/AIM client can open them:
//
//   Photo: https://litter.catbox.moe/abc123.jpg#icqnina-photo=1600x1200
//   Voice message (0:12): https://litter.catbox.moe/abc123.ogg#icqnina-voice=12300
//   Sticker: https://litter.catbox.moe/abc123.png#icqnina-sticker=hello
//
// The fragment (never sent to the web server) tells ICQ Nina what it is:
// the photo's size, the voice message's length in ms, or which of the
// app's own stickers it is (shown without downloading). A message that is
// nothing but a link to a picture or sound file on the file host counts too.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef MEDIALINK_H
#define MEDIALINK_H

#include <QString>

struct MediaLink
{
    enum Type { None = 0, Photo, Voice, Sticker };

    int type = None;
    QString url;       // the link as sent (with the fragment)
    QString info;      // photo "WxH", voice duration in ms, sticker id (may be empty)

    bool isValid() const { return type != None; }

    // Finds a media link in a received message.
    static MediaLink parse(const QString &text);
    // Hosts links are loaded from (the file host the app uploads to).
    static bool isMediaHost(const QString &host);
    // The text sent for a link (url without fragment).
    static QString messageText(int type, const QString &url, const QString &info);
    // The link with its fragment.
    static QString withFragment(int type, const QString &url, const QString &info);
    // One line for the contact list and notifications.
    static QString summary(int type, const QString &info);
    // "m:ss"
    static QString duration(qint64 ms);
    // Sticker ids are short: [a-z0-9-]{1,32}.
    static bool isValidStickerId(const QString &id);
    // Largest file loaded for each type.
    static qint64 maxBytes(int type);
};

#endif
