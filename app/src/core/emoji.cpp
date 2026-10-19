// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "emoji.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace {
const QChar kVariation(0xFE0F);

QString withoutVariation(QString s)
{
    return s.remove(kVariation);
}
} // namespace

Emoji::Emoji(QObject *parent)
    : QObject(parent)
{
}

bool Emoji::load(const QString &dir)
{
    QFile f(QDir(dir).filePath(QStringLiteral("emoji.json")));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QJsonArray categories = QJsonDocument::fromJson(f.readAll()).array();
    m_urls.clear();
    m_categories.clear();
    m_maxLength = 0;
    for (const QJsonValue &c : categories) {
        const QJsonObject category = c.toObject();
        QVariantList items;
        for (const QJsonValue &v : category.value(QStringLiteral("items")).toArray()) {
            const QJsonObject item = v.toObject();
            const QString text = item.value(QStringLiteral("t")).toString();
            const QString file = QDir(dir).filePath(item.value(QStringLiteral("f")).toString() + QStringLiteral(".png"));
            if (text.isEmpty() || !QFile::exists(file))
                continue;
            const QString url = QUrl::fromLocalFile(file).toString();
            const QString key = withoutVariation(text);
            m_urls.insert(key, url);
            m_maxLength = qMax(m_maxLength, key.size());
            QVariantMap entry;
            entry.insert(QStringLiteral("text"), text);
            entry.insert(QStringLiteral("url"), url);
            items.append(entry);
        }
        if (items.isEmpty())
            continue;
        QVariantMap entry;
        entry.insert(QStringLiteral("name"), category.value(QStringLiteral("name")).toString());
        entry.insert(QStringLiteral("items"), items);
        m_categories.append(entry);
    }
    emit loaded();
    return !m_urls.isEmpty();
}

int Emoji::match(const QString &text, int pos, QString *url) const
{
    for (int len = qMin(m_maxLength, text.size() - pos); len > 0; --len) {
        // Never split a surrogate pair.
        if (len < text.size() - pos && text.at(pos + len).isLowSurrogate())
            continue;
        const auto it = m_urls.constFind(text.mid(pos, len));
        if (it != m_urls.constEnd()) {
            if (url)
                *url = *it;
            return len;
        }
    }
    return 0;
}

QString Emoji::urlOf(const QString &emoji) const
{
    return m_urls.value(withoutVariation(emoji));
}

QString Emoji::emojiText(const QString &raw, int size) const
{
    const QString text = withoutVariation(raw);
    QString out;
    out.reserve(text.size() + 16);
    int plainStart = 0;
    auto flush = [&](int end) {
        if (end > plainStart)
            out += text.mid(plainStart, end - plainStart).toHtmlEscaped();
    };
    for (int i = 0; i < text.size();) {
        QString url;
        const int len = m_urls.isEmpty() ? 0 : match(text, i, &url);
        if (len > 0) {
            flush(i);
            out += QStringLiteral("<img src=\"%1\" width=\"%2\" height=\"%2\" align=\"middle\">").arg(url).arg(size);
            i += len;
            plainStart = i;
        } else {
            i += text.at(i).isHighSurrogate() ? 2 : 1;
        }
    }
    flush(text.size());
    out.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    return out;
}

QString Emoji::richText(const QString &text, int size) const
{
    static const QRegularExpression linkRe(QStringLiteral("(https?://|www\\.)[^\\s<>\"]+"),
                                           QRegularExpression::CaseInsensitiveOption);
    QString out;
    int last = 0;
    QRegularExpressionMatchIterator it = linkRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        QString link = m.captured(0);
        // Trailing punctuation is usually not part of the link.
        while (!link.isEmpty() && QStringLiteral(".,;:!?)]}'").contains(link.at(link.size() - 1)))
            link.chop(1);
        if (link.isEmpty())
            continue;
        out += emojiText(text.mid(last, m.capturedStart() - last), size);
        const QString href = link.startsWith(QLatin1String("www."), Qt::CaseInsensitive)
                ? QStringLiteral("http://") + link : link;
        out += QStringLiteral("<a href=\"%1\">%2</a>").arg(href.toHtmlEscaped(), link.toHtmlEscaped());
        last = m.capturedStart() + link.size();
    }
    out += emojiText(text.mid(last), size);
    return out;
}

int Emoji::emojiOnly(const QString &raw) const
{
    const QString text = withoutVariation(raw).trimmed();
    if (text.isEmpty())
        return 0;
    int count = 0;
    for (int i = 0; i < text.size();) {
        if (text.at(i).isSpace()) {
            ++i;
            continue;
        }
        const int len = match(text, i, nullptr);
        if (len == 0 || ++count > 3)
            return 0;
        i += len;
    }
    return count;
}
