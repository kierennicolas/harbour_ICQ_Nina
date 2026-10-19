// Photos, voice messages and stickers in conversations. They are uploaded to
// the temporary file host and sent as a link (MediaLink), which any client
// can open; ICQ Nina shows them in the conversation. Received ones are
// loaded into the app's own folder (automatically from people in your
// contact list), so they stay after the link expires.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef MEDIAMANAGER_H
#define MEDIAMANAGER_H

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>

#include "historystore.h"
#include "stickerstore.h"

class AccountManager;
class LinkUpload;
class QNetworkReply;

class MediaManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString mediaDir READ mediaDir CONSTANT)
    Q_PROPERTY(StickerStore *stickers READ stickers CONSTANT)

public:
    static const int LinkHours = 72;
    static const int PhotoMaxSide = 1600;

    MediaManager(AccountManager *account, const QString &mediaDir, StickerStore *stickers);
    ~MediaManager() override;

    QString mediaDir() const { return m_dir; }
    StickerStore *stickers() const { return m_stickers; }

    // pathOrUrl: local path or file:// URL. The photo is made at most
    // 1600 px (which also drops its location and other metadata). False if
    // it cannot be sent (lastError says why).
    Q_INVOKABLE bool sendPhoto(const QString &uin, const QString &pathOrUrl);
    Q_INVOKABLE bool sendVoice(const QString &uin, const QString &pathOrUrl, int durationMs);
    Q_INVOKABLE bool sendSticker(const QString &uin, const QString &stickerId);
    // Loads a received photo, voice message or sticker.
    Q_INVOKABLE void load(const QString &uin, qint64 messageId);
    // Tries a failed upload or download again.
    Q_INVOKABLE void retry(const QString &uin, qint64 messageId);
    Q_INVOKABLE void cancel(const QString &uin, qint64 messageId);
    // Copies a photo to Pictures/ICQ Nina. Returns the new path ("" on failure).
    Q_INVOKABLE QString saveToPictures(const QString &pathOrUrl);
    // A new file name in the app's media folder (camera photos, recordings),
    // without extension if extension is empty.
    Q_INVOKABLE QString newFilePath(const QString &prefix, const QString &extension = QString()) const;
    // Deletes a file of the media folder that is no longer needed (a camera
    // photo once its smaller copy is sent).
    Q_INVOKABLE void discard(const QString &pathOrUrl);
    // Turns a picture of the media folder (a camera photo) by degrees
    // clockwise (90, 180, 270), keeping it upright for any viewer.
    Q_INVOKABLE bool rotateImage(const QString &pathOrUrl, int degrees);
    Q_INVOKABLE QString lastError() const { return m_lastError; }

    // For AccountManager: a received message that carries a media link
    // becomes a media message (kind, type, link; a sticker you have is shown
    // right away). Returns false if it is an ordinary message.
    bool classify(StoredMessage &m) const;
    // Deletes the local copies of a deleted conversation.
    void forget(const QStringList &paths);

signals:
    void failed(const QString &uin, const QString &message);

private:
    qint64 addOutgoing(const QString &uin, int type, const QString &info, const QString &path, int state);
    void upload(const QString &uin, qint64 id);
    void finishSend(const QString &uin, qint64 id, const QString &url);
    void download(const QString &uin, const StoredMessage &m);
    void fail(const QString &uin, qint64 id, const QString &path, const QString &url, const QString &why);
    QString makePhoto(const QString &path, QString *info);
    static QString localPath(const QString &pathOrUrl);

    AccountManager *m_account;
    QString m_dir;
    StickerStore *m_stickers;
    QNetworkAccessManager m_network;
    QHash<qint64, QPointer<LinkUpload>> m_uploads;
    QHash<qint64, QPointer<QNetworkReply>> m_downloads;
    QString m_lastError;
};

#endif
