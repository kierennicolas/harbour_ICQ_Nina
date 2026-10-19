// Stickers: the app's own pack (read-only, installed with the app) and the
// ones you add from your pictures or from stickers people send you.
// Sticker ids travel in the message link (see MediaLink), so a sticker the
// other person also has is shown without downloading it. Ids: the pack's
// names ("hello"), "u-" + a hash of the picture for added ones.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef STICKERSTORE_H
#define STICKERSTORE_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QHash>
#include <QVector>

class StickerStore : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles { IdRole = Qt::UserRole + 1, UrlRole, BundledRole, PathRole };
    static const int MaxSide = 256;

    explicit StickerStore(QObject *parent = nullptr);

    // bundledDir: the app's pack; userDir: where added stickers are kept.
    void open(const QString &bundledDir, const QString &userDir);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds a picture as a sticker (scaled to fit 256x256, kept as PNG).
    // keepId: the id it came with in a message, kept when valid. Returns the
    // id ("" on failure, lastError says why; an existing id if already there).
    Q_INVOKABLE QString add(const QString &pathOrUrl, const QString &keepId = QString());
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool contains(const QString &id) const;
    Q_INVOKABLE bool isBundled(const QString &id) const;
    Q_INVOKABLE QString pathOf(const QString &id) const;
    Q_INVOKABLE QString urlOf(const QString &id) const;
    Q_INVOKABLE QString lastError() const { return m_lastError; }

    // A link the sticker was uploaded to, while it still works for a while.
    QString cachedLink(const QString &id) const;
    void setCachedLink(const QString &id, const QString &url, int hours);

signals:
    void countChanged();

private:
    struct Sticker { QString id; QString path; bool bundled; };
    void saveLinks() const;
    int rowOf(const QString &id) const;

    QString m_userDir;
    QVector<Sticker> m_stickers;
    QHash<QString, QPair<QString, QDateTime>> m_links; // id -> (url, expires)
    QString m_lastError;
};

#endif
