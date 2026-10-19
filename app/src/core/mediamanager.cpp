// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "mediamanager.h"

#include "accountmanager.h"
#include "linkupload.h"
#include "medialink.h"
#include "stickerstore.h"
#include "transfermanager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QTransform>
#include <QUrl>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

namespace {
const int kStallMs = 60 * 1000;

QString stamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
}

bool isVoiceFile(const QString &path)
{
    static const QStringList ok = { QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
                                    QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("mp3"),
                                    QStringLiteral("wav"), QStringLiteral("amr") };
    return ok.contains(QFileInfo(path).suffix().toLower());
}

QUrl downloadUrl(const QString &link)
{
    QUrl url(link);
    url.setFragment(QString());
#ifdef ICQNINA_TESTING
    // Tests: the fake file host hands out https://<test host>/ links and
    // serves them over plain HTTP on localhost.
    const QByteArray base = qgetenv("ICQNINA_TEST_DOWNLOAD_BASE");
    if (!base.isEmpty()) {
        const QUrl b(QString::fromUtf8(base));
        url.setScheme(b.scheme());
        url.setHost(b.host());
        url.setPort(b.port());
    }
#endif
    return url;
}
} // namespace

MediaManager::MediaManager(AccountManager *account, const QString &mediaDir, StickerStore *stickers)
    : QObject(account), m_account(account), m_dir(mediaDir), m_stickers(stickers)
{
    QDir().mkpath(m_dir);
}

MediaManager::~MediaManager()
{
    for (const QPointer<LinkUpload> &u : m_uploads)
        if (u)
            u->disconnect(this);
    for (const QPointer<QNetworkReply> &r : m_downloads)
        if (r) {
            r->disconnect(this);
            r->abort();
        }
}

QString MediaManager::localPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    return url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
}

qint64 MediaManager::addOutgoing(const QString &uin, int type, const QString &info, const QString &path, int state)
{
    StoredMessage m;
    m.contact = uin;
    m.outgoing = true;
    m.kind = StoredMessage::Media;
    m.mediaType = type;
    m.mediaInfo = info;
    m.filePath = path;
    m.fileName = QFileInfo(path).fileName();
    m.fileSize = QFileInfo(path).size();
    m.fileState = state;
    m.text = MediaLink::summary(type, info);
    m.timestamp = QDateTime::currentDateTime();
    return m_account->addFileMessage(m);
}

QString MediaManager::makePhoto(const QString &path, QString *info)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (path.isEmpty() || !reader.canRead()) {
        m_lastError = tr("Cannot read that picture.");
        return QString();
    }
    // Animated GIFs go as they are (re-encoding would stop them moving).
    if (reader.format() == "gif" && reader.supportsAnimation() && reader.imageCount() > 1
            && QFileInfo(path).size() <= 8 * 1000 * 1000) {
        const QSize size = reader.size();
        const QString target = QDir(m_dir).filePath(QStringLiteral("photo-%1.gif").arg(stamp()));
        if (!QFile::copy(path, target)) {
            m_lastError = tr("Cannot copy the picture.");
            return QString();
        }
        *info = QStringLiteral("%1x%2").arg(size.width()).arg(size.height());
        return target;
    }
    QImage image = reader.read();
    if (image.isNull()) {
        m_lastError = tr("Cannot read that picture.");
        return QString();
    }
    if (image.width() > PhotoMaxSide || image.height() > PhotoMaxSide)
        image = image.scaled(PhotoMaxSide, PhotoMaxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (image.hasAlphaChannel()) {
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, image);
        p.end();
        image = flat;
    }
    const QString target = QDir(m_dir).filePath(QStringLiteral("photo-%1.jpg").arg(stamp()));
    if (!image.save(target, "JPEG", 85)) {
        m_lastError = tr("Cannot save the picture.");
        return QString();
    }
    *info = QStringLiteral("%1x%2").arg(image.width()).arg(image.height());
    return target;
}

bool MediaManager::sendPhoto(const QString &uin, const QString &pathOrUrl)
{
    m_lastError.clear();
    QString info;
    const QString photo = makePhoto(localPath(pathOrUrl), &info);
    if (photo.isEmpty())
        return false;
    const qint64 id = addOutgoing(uin, MediaLink::Photo, info, photo, TransferManager::Uploading);
    upload(uin, id);
    return true;
}

bool MediaManager::sendVoice(const QString &uin, const QString &pathOrUrl, int durationMs)
{
    m_lastError.clear();
    QString path = localPath(pathOrUrl);
    const QFileInfo fi(path);
    if (path.isEmpty() || !fi.isFile() || !fi.isReadable() || fi.size() == 0) {
        m_lastError = tr("The recording is empty.");
        return false;
    }
    if (!isVoiceFile(path)) {
        m_lastError = tr("That is not a sound file the app can send.");
        return false;
    }
    if (fi.size() > MediaLink::maxBytes(MediaLink::Voice)) {
        m_lastError = tr("The recording is too long.");
        return false;
    }
    if (!fi.absoluteFilePath().startsWith(QDir(m_dir).absolutePath() + QLatin1Char('/'))) {
        const QString copy = QDir(m_dir).filePath(QStringLiteral("voice-%1.%2").arg(stamp(), fi.suffix().toLower()));
        if (!QFile::copy(path, copy)) {
            m_lastError = tr("Cannot copy the recording.");
            return false;
        }
        path = copy;
    }
    const qint64 id = addOutgoing(uin, MediaLink::Voice, durationMs > 0 ? QString::number(durationMs) : QString(),
                                  path, TransferManager::Uploading);
    upload(uin, id);
    return true;
}

bool MediaManager::sendSticker(const QString &uin, const QString &stickerId)
{
    m_lastError.clear();
    const QString path = m_stickers->pathOf(stickerId);
    if (path.isEmpty()) {
        m_lastError = tr("That sticker is gone.");
        return false;
    }
    const qint64 id = addOutgoing(uin, MediaLink::Sticker, stickerId, path, TransferManager::Uploading);
    const QString cached = m_stickers->cachedLink(stickerId);
    if (!cached.isEmpty())
        finishSend(uin, id, cached);
    else
        upload(uin, id);
    return true;
}

void MediaManager::upload(const QString &uin, qint64 id)
{
    const StoredMessage m = m_account->storedMessage(id);
    if (!m.id)
        return;
    if (!QFile::exists(m.filePath)) {
        fail(uin, id, m.filePath, QString(), tr("The file is gone."));
        return;
    }
    LinkUpload *up = new LinkUpload(this);
    m_uploads.insert(id, up);
    m_account->updateFileMessage(uin, id, TransferManager::Uploading, m.filePath, QString());
    m_account->setFileMessageProgress(uin, id, 0);
    connect(up, &LinkUpload::progress, this, [this, uin, id](qint64 sent, qint64) {
        m_account->setFileMessageProgress(uin, id, sent);
    });
    connect(up, &LinkUpload::failed, this, [this, uin, id, up](const QString &why) {
        up->deleteLater();
        m_uploads.remove(id);
        fail(uin, id, m_account->storedMessage(id).filePath, QString(), why);
    });
    connect(up, &LinkUpload::finished, this, [this, uin, id, up](const QString &url) {
        up->deleteLater();
        m_uploads.remove(id);
        const StoredMessage m = m_account->storedMessage(id);
        if (m.mediaType == MediaLink::Sticker)
            m_stickers->setCachedLink(m.mediaInfo, url, LinkHours);
        finishSend(uin, id, url);
    });
    up->start(m.filePath, LinkHours);
}

void MediaManager::finishSend(const QString &uin, qint64 id, const QString &url)
{
    const StoredMessage m = m_account->storedMessage(id);
    if (!m.id)
        return;
    const QString link = MediaLink::withFragment(m.mediaType, url, m.mediaInfo);
    if (m_account->sendText(uin, MediaLink::messageText(m.mediaType, url, m.mediaInfo), false, id))
        m_account->updateFileMessage(uin, id, TransferManager::Done, m.filePath, link);
    else
        fail(uin, id, m.filePath, link, tr("Uploaded, but you were offline, so it was not sent. Try again when you are online."));
}

void MediaManager::fail(const QString &uin, qint64 id, const QString &path, const QString &url, const QString &why)
{
    m_account->updateFileMessage(uin, id, TransferManager::Failed, path, url);
    m_lastError = why;
    emit failed(uin, why);
}

bool MediaManager::classify(StoredMessage &m) const
{
    const MediaLink link = MediaLink::parse(m.text);
    if (!link.isValid())
        return false;
    m.kind = StoredMessage::Media;
    m.mediaType = link.type;
    m.mediaInfo = link.info;
    m.fileUrl = link.url;
    m.fileState = TransferManager::Offered;
    if (link.type == MediaLink::Sticker && !link.info.isEmpty() && m_stickers->contains(link.info)) {
        m.filePath = m_stickers->pathOf(link.info);
        m.fileState = TransferManager::Done;
    }
    return true;
}

void MediaManager::load(const QString &uin, qint64 messageId)
{
    const StoredMessage m = m_account->storedMessage(messageId);
    if (!m.id || m.kind != StoredMessage::Media || m.outgoing || m_downloads.contains(messageId))
        return;
    if (m.fileState == TransferManager::Done && QFile::exists(m.filePath))
        return;
    download(uin, m);
}

void MediaManager::download(const QString &uin, const StoredMessage &m)
{
    const qint64 id = m.id;
    const QUrl link(m.fileUrl);
    if (!link.isValid() || !MediaLink::isMediaHost(link.host())) {
        fail(uin, id, QString(), m.fileUrl, tr("Not loaded: the link is not on the app's file host."));
        return;
    }
    const QUrl url = downloadUrl(m.fileUrl);
    QString name = QFileInfo(url.path()).fileName();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    if (name.isEmpty() || name.startsWith(QLatin1Char('.')))
        name = QStringLiteral("media") + name;
    const QString target = QDir(m_dir).filePath(QStringLiteral("in-%1-%2").arg(id).arg(name.right(64)));
    const qint64 max = MediaLink::maxBytes(m.mediaType);
    const int type = m.mediaType;

    QFile *file = new QFile(target + QStringLiteral(".part"));
    if (!file->open(QIODevice::WriteOnly)) {
        delete file;
        fail(uin, id, QString(), m.fileUrl, tr("Cannot save it."));
        return;
    }

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "ICQNina-Sailfish/" APP_VERSION);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    m_network.setNetworkAccessible(QNetworkAccessManager::Accessible);
#endif
    QNetworkReply *reply = m_network.get(request);
    file->setParent(reply);
    m_downloads.insert(id, reply);
    m_account->updateFileMessage(uin, id, TransferManager::Transferring, QString(), m.fileUrl);
    m_account->setFileMessageProgress(uin, id, 0);

    QTimer *watchdog = new QTimer(reply);
    watchdog->setSingleShot(true);
    watchdog->start(kStallMs);
    connect(watchdog, &QTimer::timeout, reply, [reply]() {
        reply->setProperty("why", tr("The download stalled."));
        reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [reply, max](qint64, qint64 total) {
        if (total > max) {
            reply->setProperty("why", tr("It is too big to load."));
            reply->abort();
        }
    });
    connect(reply, &QNetworkReply::readyRead, this, [this, uin, id, reply, file, watchdog, max]() {
        watchdog->start(kStallMs);
        const QByteArray data = reply->readAll();
        if (file->size() + data.size() > max) {
            reply->setProperty("why", tr("It is too big to load."));
            reply->abort();
            return;
        }
        file->write(data);
        m_account->setFileMessageProgress(uin, id, file->size());
    });
    connect(reply, &QNetworkReply::finished, this, [this, uin, id, reply, file, target, type]() {
        reply->deleteLater();
        m_downloads.remove(id);
        const QString link = m_account->storedMessage(id).fileUrl;
        if (reply->property("cancelled").toBool()) {
            file->remove();
            m_account->updateFileMessage(uin, id, TransferManager::Offered, QString(), link);
            return;
        }
        if (reply->error() == QNetworkReply::NoError)
            file->write(reply->readAll());
        file->close();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString why = reply->property("why").toString();
        if (why.isEmpty() && (status == 404 || status == 410))
            why = tr("The link has expired.");
        else if (why.isEmpty() && reply->error() != QNetworkReply::NoError)
            why = reply->errorString();
        else if (why.isEmpty() && status != 200)
            why = tr("The file host answered HTTP %1.").arg(status);
        else if (why.isEmpty() && file->size() == 0)
            why = tr("The file is empty.");
        if (why.isEmpty() && (type == MediaLink::Photo || type == MediaLink::Sticker)
                && !QImageReader(file->fileName()).canRead())
            why = tr("It is not a picture.");
        if (!why.isEmpty()) {
            file->remove();
            fail(uin, id, QString(), link, why);
            return;
        }
        QFile::remove(target);
        if (!file->rename(target)) {
            file->remove();
            fail(uin, id, QString(), link, tr("Cannot save it."));
            return;
        }
        m_account->updateFileMessage(uin, id, TransferManager::Done, target, link, QFileInfo(target).size());
    });
}

void MediaManager::retry(const QString &uin, qint64 messageId)
{
    const StoredMessage m = m_account->storedMessage(messageId);
    if (!m.id || m.kind != StoredMessage::Media || m_uploads.contains(messageId) || m_downloads.contains(messageId))
        return;
    if (!m.outgoing) {
        download(uin, m);
        return;
    }
    // Uploaded, but not sent: the link still works for a while.
    if (!m.fileUrl.isEmpty() && m.timestamp.secsTo(QDateTime::currentDateTime()) < (LinkHours - 12) * 3600) {
        finishSend(uin, messageId, m.fileUrl.section(QLatin1Char('#'), 0, 0));
        return;
    }
    upload(uin, messageId);
}

void MediaManager::cancel(const QString &, qint64 messageId)
{
    if (QPointer<LinkUpload> up = m_uploads.value(messageId))
        up->cancel();
    if (QPointer<QNetworkReply> reply = m_downloads.value(messageId)) {
        reply->setProperty("cancelled", true);
        reply->abort();
    }
}

QString MediaManager::saveToPictures(const QString &pathOrUrl)
{
    m_lastError.clear();
    const QString path = localPath(pathOrUrl);
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + QStringLiteral("/Pictures");
    dir += QStringLiteral("/ICQ Nina");
    QDir().mkpath(dir);
    QString name = QFileInfo(path).fileName();
    name.remove(QRegularExpression(QStringLiteral("^in-[0-9]+-")));
    const QString target = TransferManager::uniquePath(dir, name);
    if (!QFile::exists(path) || !QFile::copy(path, target)) {
        m_lastError = tr("Cannot save the picture.");
        return QString();
    }
    return target;
}

QString MediaManager::newFilePath(const QString &prefix, const QString &extension) const
{
    QString name = prefix;
    name.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")));
    name = QStringLiteral("%1-%2").arg(name.isEmpty() ? QStringLiteral("file") : name, stamp());
    if (!extension.isEmpty())
        name += QLatin1Char('.') + extension;
    return QDir(m_dir).filePath(name);
}

bool MediaManager::rotateImage(const QString &pathOrUrl, int degrees)
{
    m_lastError.clear();
    const QString path = localPath(pathOrUrl);
    degrees = ((degrees % 360) + 360) % 360;
    if (!QFileInfo(path).absoluteFilePath().startsWith(QDir(m_dir).absolutePath() + QLatin1Char('/'))) {
        m_lastError = tr("Cannot change that picture.");
        return false;
    }
    if (degrees == 0)
        return true;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        m_lastError = tr("Cannot read that picture.");
        return false;
    }
    QTransform turn;
    turn.rotate(degrees);
    // Saved without the original's orientation mark: the pixels are upright.
    if (!image.transformed(turn).save(path, "JPEG", 92)) {
        m_lastError = tr("Cannot save the picture.");
        return false;
    }
    return true;
}

void MediaManager::discard(const QString &pathOrUrl)
{
    forget({ localPath(pathOrUrl) });
}

void MediaManager::forget(const QStringList &paths)
{
    const QString dir = QDir(m_dir).absolutePath() + QLatin1Char('/');
    for (const QString &p : paths)
        if (QFileInfo(p).absoluteFilePath().startsWith(dir))
            QFile::remove(p);
}
