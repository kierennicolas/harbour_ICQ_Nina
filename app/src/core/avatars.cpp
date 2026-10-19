// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "avatars.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QUrl>

namespace {
const int kRoundSize = 128;
}

bool Avatars::isClearHash(const QByteArray &hash)
{
    static const QByteArray clear = QByteArray::fromHex("0201d20472");
    return hash.isEmpty() || hash == clear;
}

void Avatars::open(const QString &dir)
{
    m_dir = dir;
    m_hashes.clear();
    QDir().mkpath(dir);
    QSettings s(dir + QStringLiteral("/index.ini"), QSettings::IniFormat);
    s.beginGroup(QStringLiteral("contacts"));
    for (const QString &key : s.childKeys())
        m_hashes.insert(QString::fromUtf8(QByteArray::fromHex(key.toLatin1())),
                        QByteArray::fromHex(s.value(key).toByteArray()));
    s.endGroup();
}

void Avatars::save() const
{
    QSettings s(m_dir + QStringLiteral("/index.ini"), QSettings::IniFormat);
    s.remove(QStringLiteral("contacts"));
    s.beginGroup(QStringLiteral("contacts"));
    // Keys are hex so any screen name is a valid INI key.
    for (auto it = m_hashes.constBegin(); it != m_hashes.constEnd(); ++it)
        s.setValue(QString::fromLatin1(it.key().toUtf8().toHex()), it.value().toHex());
    s.endGroup();
}

void Avatars::setHash(const QString &contact, const QByteArray &hash)
{
    if (m_hashes.value(contact) == hash)
        return;
    if (isClearHash(hash))
        m_hashes.remove(contact);
    else
        m_hashes.insert(contact, hash);
    save();
}

QString Avatars::pathFor(const QByteArray &hash, const char *suffix) const
{
    return QStringLiteral("%1/%2%3.png").arg(m_dir, QString::fromLatin1(hash.toHex()), QString::fromLatin1(suffix));
}

QString Avatars::roundUrl(const QByteArray &hash) const
{
    if (isClearHash(hash) || m_dir.isEmpty())
        return QString();
    const QString p = pathFor(hash, "-round");
    return QFileInfo::exists(p) ? QUrl::fromLocalFile(p).toString() : QString();
}

QString Avatars::squareUrl(const QByteArray &hash) const
{
    if (isClearHash(hash) || m_dir.isEmpty())
        return QString();
    const QString p = pathFor(hash, "");
    return QFileInfo::exists(p) ? QUrl::fromLocalFile(p).toString() : QString();
}

QString Avatars::store(const QByteArray &hash, const QByteArray &data)
{
    QImage image;
    if (m_dir.isEmpty() || isClearHash(hash) || !image.loadFromData(data) || image.isNull())
        return QString();
    image.save(pathFor(hash, ""), "PNG");

    // Round version: centre square, scaled, clipped to a circle.
    const int side = qMin(image.width(), image.height());
    const QImage square = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
                               .scaled(kRoundSize, kRoundSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QImage round(kRoundSize, kRoundSize, QImage::Format_ARGB32_Premultiplied);
    round.fill(Qt::transparent);
    QPainter p(&round);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QPainterPath circle;
    circle.addEllipse(0, 0, kRoundSize, kRoundSize);
    p.setClipPath(circle);
    p.drawImage(0, 0, square);
    p.end();
    round.save(pathFor(hash, "-round"), "PNG");
    return roundUrl(hash);
}

QByteArray Avatars::makeIcon(const QString &path, int size, int maxBytes, QString *error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        if (error)
            *error = QObject::tr("Cannot read that picture.");
        return QByteArray();
    }
    const int side = qMin(image.width(), image.height());
    image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
                 .scaled(size, size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                 .convertToFormat(QImage::Format_RGB32);
    for (int quality = 90; quality >= 30; quality -= 10) {
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", quality);
        if (data.size() <= maxBytes)
            return data;
    }
    if (error)
        *error = QObject::tr("The picture is too detailed to make a small enough icon.");
    return QByteArray();
}
