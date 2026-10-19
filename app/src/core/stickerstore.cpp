// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "stickerstore.h"

#include "medialink.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QSettings>
#include <QUrl>

namespace {
QString localPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

// Pack order: friendliest first.
const char *const kPackOrder[] = { "hello", "lol", "love", "ok", "thanks", "wow", "party",
                                   "coffee", "sleepy", "sad", "what", "angry" };
} // namespace

StickerStore::StickerStore(QObject *parent)
    : QAbstractListModel(parent)
{
}

void StickerStore::open(const QString &bundledDir, const QString &userDir)
{
    beginResetModel();
    m_stickers.clear();
    m_links.clear();
    m_userDir = userDir;

    QStringList bundled;
    const QDir pack(bundledDir);
    if (!bundledDir.isEmpty() && pack.exists()) {
        const QStringList files = pack.entryList({ QStringLiteral("*.png") }, QDir::Files, QDir::Name);
        for (const char *name : kPackOrder)
            if (files.contains(QLatin1String(name) + QStringLiteral(".png")))
                bundled << QLatin1String(name);
        for (const QString &f : files) {
            const QString id = f.left(f.size() - 4);
            if (!bundled.contains(id) && MediaLink::isValidStickerId(id))
                bundled << id;
        }
    }
    for (const QString &id : bundled)
        m_stickers.append({ id, pack.filePath(id + QStringLiteral(".png")), true });

    if (!userDir.isEmpty()) {
        QDir().mkpath(userDir);
        QFileInfoList mine = QDir(userDir).entryInfoList({ QStringLiteral("u-*.png") }, QDir::Files, QDir::Time | QDir::Reversed);
        for (const QFileInfo &fi : mine) {
            const QString id = fi.completeBaseName();
            if (MediaLink::isValidStickerId(id))
                m_stickers.append({ id, fi.absoluteFilePath(), false });
        }
        QSettings s(userDir + QStringLiteral("/links.ini"), QSettings::IniFormat);
        const QDateTime now = QDateTime::currentDateTimeUtc();
        for (const QString &id : s.childGroups()) {
            s.beginGroup(id);
            const QString url = s.value(QStringLiteral("url")).toString();
            const QDateTime expires = s.value(QStringLiteral("expires")).toDateTime();
            s.endGroup();
            if (!url.isEmpty() && expires > now)
                m_links.insert(id, qMakePair(url, expires));
        }
    }
    endResetModel();
    emit countChanged();
}

int StickerStore::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_stickers.size();
}

QVariant StickerStore::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_stickers.size())
        return QVariant();
    const Sticker &s = m_stickers.at(index.row());
    switch (role) {
    case IdRole: return s.id;
    case UrlRole: return QUrl::fromLocalFile(s.path).toString();
    case BundledRole: return s.bundled;
    case PathRole: return s.path;
    }
    return QVariant();
}

QHash<int, QByteArray> StickerStore::roleNames() const
{
    return { { IdRole, "stickerId" }, { UrlRole, "url" }, { BundledRole, "bundled" }, { PathRole, "path" } };
}

int StickerStore::rowOf(const QString &id) const
{
    for (int i = 0; i < m_stickers.size(); ++i)
        if (m_stickers.at(i).id == id)
            return i;
    return -1;
}

bool StickerStore::contains(const QString &id) const
{
    return rowOf(id) >= 0;
}

bool StickerStore::isBundled(const QString &id) const
{
    const int row = rowOf(id);
    return row >= 0 && m_stickers.at(row).bundled;
}

QString StickerStore::pathOf(const QString &id) const
{
    const int row = rowOf(id);
    return row >= 0 ? m_stickers.at(row).path : QString();
}

QString StickerStore::urlOf(const QString &id) const
{
    const QString path = pathOf(id);
    return path.isEmpty() ? QString() : QUrl::fromLocalFile(path).toString();
}

QString StickerStore::add(const QString &pathOrUrl, const QString &keepId)
{
    m_lastError.clear();
    if (m_userDir.isEmpty()) {
        m_lastError = tr("Stickers are not available.");
        return QString();
    }
    const QString path = localPath(pathOrUrl);
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        m_lastError = tr("Cannot read that picture.");
        return QString();
    }
    if (image.width() > MaxSide || image.height() > MaxSide)
        image = image.scaled(MaxSide, MaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    image = image.convertToFormat(QImage::Format_ARGB32);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        m_lastError = tr("Cannot make a sticker from that picture.");
        return QString();
    }

    QString id = keepId;
    if (!id.startsWith(QLatin1String("u-")) || !MediaLink::isValidStickerId(id))
        id = QStringLiteral("u-") + QString::fromLatin1(QCryptographicHash::hash(png, QCryptographicHash::Sha1).toHex().left(16));
    if (contains(id))
        return id;

    const QString target = QDir(m_userDir).filePath(id + QStringLiteral(".png"));
    QFile f(target);
    if (!f.open(QIODevice::WriteOnly) || f.write(png) != png.size()) {
        f.remove();
        m_lastError = tr("Cannot save the sticker.");
        return QString();
    }
    f.close();
    beginInsertRows(QModelIndex(), m_stickers.size(), m_stickers.size());
    m_stickers.append({ id, target, false });
    endInsertRows();
    emit countChanged();
    return id;
}

bool StickerStore::remove(const QString &id)
{
    const int row = rowOf(id);
    if (row < 0 || m_stickers.at(row).bundled)
        return false;
    QFile::remove(m_stickers.at(row).path);
    beginRemoveRows(QModelIndex(), row, row);
    m_stickers.remove(row);
    endRemoveRows();
    if (m_links.remove(id))
        saveLinks();
    emit countChanged();
    return true;
}

QString StickerStore::cachedLink(const QString &id) const
{
    const auto it = m_links.constFind(id);
    // Leave some time for the other person to open it.
    if (it == m_links.constEnd() || QDateTime::currentDateTimeUtc().secsTo(it->second) < 12 * 3600)
        return QString();
    return it->first;
}

void StickerStore::setCachedLink(const QString &id, const QString &url, int hours)
{
    if (url.isEmpty() || hours <= 0)
        return;
    m_links.insert(id, qMakePair(url, QDateTime::currentDateTimeUtc().addSecs(qint64(hours) * 3600 - 300)));
    saveLinks();
}

void StickerStore::saveLinks() const
{
    if (m_userDir.isEmpty())
        return;
    QSettings s(m_userDir + QStringLiteral("/links.ini"), QSettings::IniFormat);
    s.clear();
    for (auto it = m_links.constBegin(); it != m_links.constEnd(); ++it) {
        s.beginGroup(it.key());
        s.setValue(QStringLiteral("url"), it->first);
        s.setValue(QStringLiteral("expires"), it->second);
        s.endGroup();
    }
}
