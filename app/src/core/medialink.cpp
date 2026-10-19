// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "medialink.h"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

namespace {
const QString kPrefix = QStringLiteral("icqnina-");

QString typeName(int type)
{
    switch (type) {
    case MediaLink::Photo: return QStringLiteral("photo");
    case MediaLink::Voice: return QStringLiteral("voice");
    case MediaLink::Sticker: return QStringLiteral("sticker");
    }
    return QString();
}

int typeFromExtension(const QString &path)
{
    const QString ext = path.section(QLatin1Char('.'), -1).toLower();
    static const QStringList images = { QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
                                        QStringLiteral("gif"), QStringLiteral("webp") };
    static const QStringList sounds = { QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
                                        QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("mp3"),
                                        QStringLiteral("wav"), QStringLiteral("amr") };
    if (!path.contains(QLatin1Char('.')))
        return MediaLink::None;
    if (images.contains(ext))
        return MediaLink::Photo;
    if (sounds.contains(ext))
        return MediaLink::Voice;
    return MediaLink::None;
}

QString tr(const char *text)
{
    return QCoreApplication::translate("MediaLink", text);
}
} // namespace

bool MediaLink::isMediaHost(const QString &host)
{
    const QString h = host.toLower();
    if (h == QLatin1String("litter.catbox.moe") || h == QLatin1String("files.catbox.moe"))
        return true;
#ifdef ICQNINA_TESTING
    const QByteArray test = qgetenv("ICQNINA_TEST_UPLOAD_HOST");
    if (!test.isEmpty() && h == QString::fromUtf8(test).section(QLatin1Char(':'), 0, 0).toLower())
        return true;
#endif
    return false;
}

bool MediaLink::isValidStickerId(const QString &id)
{
    static const QRegularExpression re(QStringLiteral("^[a-z0-9-]{1,32}$"));
    return re.match(id).hasMatch();
}

qint64 MediaLink::maxBytes(int type)
{
    switch (type) {
    case Sticker: return 2LL * 1000 * 1000;
    case Voice: return 25LL * 1000 * 1000;
    default: return 30LL * 1000 * 1000;
    }
}

MediaLink MediaLink::parse(const QString &rawText)
{
    MediaLink link;
    // Offline messages can come back with stray control characters (a
    // terminating NUL, CR) that would end up in the link.
    static const QRegularExpression controls(QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x7F]"));
    QString text = rawText;
    text.remove(controls);
    static const QRegularExpression urlRe(QStringLiteral("https?://[^\\s<>\"']+"),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = urlRe.globalMatch(text);
    QStringList urls;
    while (it.hasNext())
        urls << it.next().captured(0);
    if (urls.size() != 1)
        return link;

    const QUrl url(urls.first(), QUrl::StrictMode);
    if (!url.isValid() || !isMediaHost(url.host()))
        return link;
#ifndef ICQNINA_TESTING
    if (url.scheme().toLower() != QLatin1String("https"))
        return link;
#endif

    const QString fragment = url.fragment(QUrl::FullyDecoded);
    if (fragment.startsWith(kPrefix)) {
        const QString kind = fragment.mid(kPrefix.size()).section(QLatin1Char('='), 0, 0);
        const QString info = fragment.section(QLatin1Char('='), 1);
        if (kind == QLatin1String("photo")) {
            link.type = Photo;
            if (QRegularExpression(QStringLiteral("^[0-9]{1,5}x[0-9]{1,5}$")).match(info).hasMatch())
                link.info = info;
        } else if (kind == QLatin1String("voice")) {
            link.type = Voice;
            bool ok = false;
            const qint64 ms = info.toLongLong(&ok);
            if (ok && ms > 0 && ms < 24LL * 3600 * 1000)
                link.info = QString::number(ms);
        } else if (kind == QLatin1String("sticker")) {
            link.type = Sticker;
            if (isValidStickerId(info))
                link.info = info;
        }
    } else if (text.trimmed() == urls.first()) {
        // A bare link to a picture or a sound file on the file host.
        link.type = typeFromExtension(url.path());
    }
    if (link.type != None)
        link.url = urls.first();
    return link;
}

QString MediaLink::withFragment(int type, const QString &url, const QString &info)
{
    const QString base = url.section(QLatin1Char('#'), 0, 0);
    QString fragment = kPrefix + typeName(type);
    if (!info.isEmpty())
        fragment += QLatin1Char('=') + info;
    return base + QLatin1Char('#') + fragment;
}

QString MediaLink::duration(qint64 ms)
{
    const qint64 secs = (ms + 500) / 1000;
    return QStringLiteral("%1:%2").arg(secs / 60).arg(secs % 60, 2, 10, QLatin1Char('0'));
}

QString MediaLink::messageText(int type, const QString &url, const QString &info)
{
    const QString link = withFragment(type, url, info);
    switch (type) {
    case Photo:
        return tr("Photo: %1").arg(link);
    case Voice:
        return info.toLongLong() > 0 ? tr("Voice message (%1): %2").arg(duration(info.toLongLong()), link)
                                     : tr("Voice message: %1").arg(link);
    case Sticker:
        return tr("Sticker: %1").arg(link);
    }
    return link;
}

QString MediaLink::summary(int type, const QString &info)
{
    switch (type) {
    case Photo:
        return tr("Photo");
    case Voice:
        return info.toLongLong() > 0 ? tr("Voice message (%1)").arg(duration(info.toLongLong()))
                                     : tr("Voice message");
    case Sticker:
        return tr("Sticker");
    }
    return QString();
}
