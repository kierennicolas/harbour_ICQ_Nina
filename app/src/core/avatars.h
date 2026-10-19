// Profile pictures (OSCAR "buddy icons"): a disk cache keyed by the icon's
// hash, round versions for lists, which contact has which icon, and turning
// a photo into an icon small enough for the server.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef AVATARS_H
#define AVATARS_H

#include <QByteArray>
#include <QHash>
#include <QString>

class Avatars
{
public:
    // A buddy icon hash that means "no icon" (AIM's "clear icon").
    static bool isClearHash(const QByteArray &hash);

    void open(const QString &dir);
    QString dir() const { return m_dir; }

    // file:// URL of the round picture for this hash, or empty if not cached.
    QString roundUrl(const QByteArray &hash) const;
    QString squareUrl(const QByteArray &hash) const;
    // Saves icon data received for hash. Returns the round URL, or empty if
    // the data is not an image.
    QString store(const QByteArray &hash, const QByteArray &data);

    // Which contact (normalized id) shows which hash.
    QByteArray hashOf(const QString &contact) const { return m_hashes.value(contact); }
    void setHash(const QString &contact, const QByteArray &hash);
    QHash<QString, QByteArray> all() const { return m_hashes; }

    // Square JPEG of at most maxBytes from an image file (photo orientation
    // applied). Returns empty data and sets error on failure.
    static QByteArray makeIcon(const QString &path, int size, int maxBytes, QString *error);

private:
    QString pathFor(const QByteArray &hash, const char *suffix) const;
    void save() const;

    QString m_dir;
    QHash<QString, QByteArray> m_hashes;
};

#endif
