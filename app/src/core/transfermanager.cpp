// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "transfermanager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#include "accountmanager.h"
#include "historystore.h"
#include "linkupload.h"

#include "client.h"
#include "filetransferhandler.h"
#include "oscarutils.h"
#include <kio/global.h>

TransferManager::TransferManager(AccountManager *account, const QString &downloadDir)
    : QAbstractListModel(account), m_account(account), m_downloadDir(downloadDir)
{
}

TransferManager::~TransferManager()
{
    for (const Item &it : m_items)
        if (it.upload)
            it.upload->disconnect(this);
}

int TransferManager::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TransferManager::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();
    const Item &it = m_items.at(index.row());
    switch (role) {
    case IdRole: return it.id;
    case ContactRole: return it.contact;
    case IncomingRole: return it.incoming;
    case FileNameRole: return it.fileName;
    case FileSizeRole: return it.size;
    case DoneRole: return it.done;
    case StateRole: return it.state;
    case PathRole: return it.path;
    case UrlRole: return it.url;
    case ErrorRole: return it.error;
    case MethodRole: return it.method;
    }
    return QVariant();
}

QHash<int, QByteArray> TransferManager::roleNames() const
{
    return {
        { IdRole, "transferId" }, { ContactRole, "contact" }, { IncomingRole, "incoming" },
        { FileNameRole, "fileName" }, { FileSizeRole, "fileSize" }, { DoneRole, "done" },
        { StateRole, "state" }, { PathRole, "path" }, { UrlRole, "url" }, { ErrorRole, "error" },
        { MethodRole, "method" },
    };
}

bool TransferManager::isActive(int state)
{
    return state == Offered || state == Waiting || state == Transferring || state == Uploading;
}

int TransferManager::activeCount() const
{
    int n = 0;
    for (const Item &it : m_items)
        if (isActive(it.state))
            ++n;
    return n;
}

int TransferManager::rowOf(const QString &id) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items.at(i).id == id)
            return i;
    return -1;
}

TransferManager::Item *TransferManager::find(const QString &id)
{
    const int row = rowOf(id);
    return row < 0 ? nullptr : &m_items[row];
}

void TransferManager::add(Item item)
{
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(item);
    endInsertRows();
    emit activeCountChanged();
}

void TransferManager::changed(int row)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
}

QString TransferManager::formatSize(qint64 size)
{
    if (size < 1024)
        return tr("%1 B").arg(size);
    if (size < 1024 * 1024)
        return tr("%1 KB").arg(QString::number(double(size) / 1024.0, 'f', 1));
    if (size < 1024LL * 1024 * 1024)
        return tr("%1 MB").arg(QString::number(double(size) / (1024.0 * 1024), 'f', 1));
    return tr("%1 GB").arg(QString::number(double(size) / (1024.0 * 1024 * 1024), 'f', 2));
}

QString TransferManager::uniquePath(const QString &dir, const QString &fileName)
{
    QString name = QFileInfo(fileName).fileName(); // never a path from the other side
    name.remove(QChar());
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
        name = QStringLiteral("file");
    const QDir d(dir);
    QString candidate = d.filePath(name);
    if (!QFileInfo::exists(candidate))
        return candidate;
    const QFileInfo fi(name);
    const QString base = fi.completeBaseName().isEmpty() ? name : fi.completeBaseName();
    const QString suffix = fi.completeBaseName().isEmpty() || fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
    for (int i = 1; i < 10000; ++i) {
        candidate = d.filePath(QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(suffix));
        if (!QFileInfo::exists(candidate))
            return candidate;
    }
    return d.filePath(QStringLiteral("%1-%2%3").arg(base).arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix));
}

QString TransferManager::localPath(const QString &pathOrUrl)
{
    if (pathOrUrl.startsWith(QLatin1String("file:"))) {
        const QUrl url(pathOrUrl);
        return url.isLocalFile() ? url.toLocalFile() : QString();
    }
    return pathOrUrl;
}

void TransferManager::setClient(Oscar::Client *client)
{
    if (m_client == client)
        return;
    m_client = client;
    if (client)
        return;
    // The connection's tasks went with it.
    for (int i = 0; i < m_items.size(); ++i) {
        const Item &it = m_items.at(i);
        if (it.method == Direct && isActive(it.state))
            setState(it.id, Failed, tr("Disconnected."));
    }
}

void TransferManager::setState(const QString &id, int state, const QString &error)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    Item &it = m_items[row];
    const bool wasActive = isActive(it.state);
    if (it.state == state && it.error == error)
        return;
    it.state = state;
    it.error = error;
    if (state == Done)
        it.done = it.size;
    changed(row);
    m_account->updateFileMessage(it.contact, it.messageId, state, it.path, it.url);
    if (wasActive != isActive(state))
        emit activeCountChanged();
    if (wasActive && !isActive(state)) {
        if (it.handler)
            it.handler->disconnect(this);
        emit transferEnded(it.id, it.contact, it.fileName, it.incoming, state);
    }
}

void TransferManager::setDone(const QString &id, qint64 done)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    Item &it = m_items[row];
    it.done = done;
    changed(row);
    m_account->setFileMessageProgress(it.contact, it.messageId, done);
}

void TransferManager::watch(FileTransferHandler *handler, const QString &id)
{
    connect(handler, &FileTransferHandler::transferProcessed, this, [this, id](unsigned int total) {
        Item *it = find(id);
        if (!it)
            return;
        if (it->state == Waiting)
            setState(id, Transferring);
        setDone(id, total);
    });
    connect(handler, &FileTransferHandler::transferFinished, this, [this, id]() {
        setState(id, Done);
    });
    connect(handler, &FileTransferHandler::transferCancelled, this, [this, id]() {
        Item *it = find(id);
        if (!it || !isActive(it->state))
            return;
        if (it->state == Offered)
            setState(id, Cancelled, tr("%1 cancelled the transfer.").arg(m_account->displayName(it->contact)));
        else if (!it->incoming && it->done == 0)
            setState(id, Declined, tr("%1 did not accept the file.").arg(m_account->displayName(it->contact)));
        else
            setState(id, Cancelled, tr("The transfer was cancelled."));
    });
    connect(handler, &FileTransferHandler::transferError, this, [this, id](int code, const QString &error) {
        QString why;
        switch (code) {
        case KIO::ERR_COULD_NOT_CONNECT:
        case KIO::ERR_COULD_NOT_LOGIN:
        case KIO::ERR_ABORTED:
        case KIO::ERR_COULD_NOT_LISTEN:
        case KIO::ERR_COULD_NOT_ACCEPT:
            why = tr("Could not connect to the other side (%1). Firewalls can block direct transfers; "
                     "sending it as a link always works.").arg(error.isEmpty() ? tr("no answer") : error);
            break;
        case KIO::ERR_CANNOT_OPEN_FOR_WRITING:
            why = tr("Cannot save the file in %1.").arg(m_downloadDir);
            break;
        case KIO::ERR_DOES_NOT_EXIST:
        case KIO::ERR_COULD_NOT_READ:
        case KIO::ERR_CANNOT_OPEN_FOR_READING:
            why = tr("Cannot read the file.");
            break;
        default:
            why = error.isEmpty() ? tr("The transfer failed.") : tr("The transfer failed: %1").arg(error);
        }
        setState(id, Failed, why);
    });
}

void TransferManager::handleIncoming(FileTransferHandler *handler)
{
    if (!handler)
        return;
    const QString from = handler->contact();
    if (m_account->isBlocked(from)) {
        handler->cancel();
        return;
    }
    Item it;
    it.id = handler->internalId();
    it.contact = from;
    it.incoming = true;
    it.fileCount = qMax(1, int(handler->fileCount()));
    it.fileName = handler->fileName().isEmpty()
            ? tr("%n file(s)", nullptr, it.fileCount) : QFileInfo(handler->fileName()).fileName();
    it.size = handler->totalSize();
    it.state = Offered;
    it.handler = handler;
    if (it.id.isEmpty() || rowOf(it.id) >= 0)
        return;

    StoredMessage m;
    m.contact = from;
    m.outgoing = false;
    m.kind = StoredMessage::File;
    m.fileName = it.fileName;
    m.fileSize = it.size;
    m.fileState = Offered;
    m.transferId = it.id;
    m.timestamp = QDateTime::currentDateTime();
    m.delivered = true;
    it.messageId = m_account->addFileMessage(m);

    add(it);
    watch(handler, it.id);
    emit incomingOffer(it.id, from, it.fileName, it.size);
}

bool TransferManager::accept(const QString &id)
{
    Item *it = find(id);
    if (!it || it->state != Offered || !it->handler)
        return false;
    QDir().mkpath(m_downloadDir);
    if (it->fileCount > 1) {
        const QString dir = uniquePath(m_downloadDir, tr("Files from %1").arg(m_account->displayName(it->contact)));
        QDir().mkpath(dir);
        it->path = dir;
        it->handler->save(dir);
    } else {
        it->path = uniquePath(m_downloadDir, it->fileName);
        it->handler->saveAs(QStringList() << it->path);
    }
    setState(id, Waiting);
    return true;
}

void TransferManager::decline(const QString &id)
{
    Item *it = find(id);
    if (!it || it->state != Offered)
        return;
    if (it->handler)
        it->handler->cancel();
    setState(id, Declined, tr("You declined the file."));
}

void TransferManager::cancel(const QString &id)
{
    Item *it = find(id);
    if (!it || !isActive(it->state))
        return;
    if (it->state == Offered) {
        decline(id);
        return;
    }
    if (it->upload)
        it->upload->cancel();
    else if (it->handler)
        it->handler->cancel();
    setState(id, Cancelled, tr("You cancelled the transfer."));
}

QString TransferManager::sendFile(const QString &uin, const QString &pathOrUrl)
{
    const QString path = localPath(pathOrUrl);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile() || !info.isReadable()) {
        m_lastError = tr("Cannot read the file.");
        return QString();
    }
    if (info.size() == 0) {
        m_lastError = tr("The file is empty.");
        return QString();
    }
    if (info.size() > qint64(0xFFFFFFFFu)) {
        m_lastError = tr("Files over 4 GB cannot be sent directly.");
        return QString();
    }
    if (!m_client || !m_account->isOnline()) {
        m_lastError = tr("Go online to send files.");
        return QString();
    }
    FileTransferHandler *handler = m_client->createFileTransfer(uin, QStringList() << info.absoluteFilePath());
    if (!handler) {
        m_lastError = tr("Cannot start the transfer.");
        return QString();
    }
    Item it;
    it.id = handler->internalId();
    it.contact = uin;
    it.incoming = false;
    it.fileName = info.fileName();
    it.size = info.size();
    it.path = info.absoluteFilePath();
    it.state = Waiting;
    it.handler = handler;

    StoredMessage m;
    m.contact = uin;
    m.outgoing = true;
    m.kind = StoredMessage::File;
    m.fileName = it.fileName;
    m.fileSize = it.size;
    m.filePath = it.path;
    m.fileState = Waiting;
    m.transferId = it.id;
    m.timestamp = QDateTime::currentDateTime();
    it.messageId = m_account->addFileMessage(m);

    add(it);
    watch(handler, it.id);
    handler->send();
    m_lastError.clear();
    return it.id;
}

QString TransferManager::sendLink(const QString &uin, const QString &pathOrUrl, int hours)
{
    const QString path = localPath(pathOrUrl);
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile() || !info.isReadable()) {
        m_lastError = tr("Cannot read the file.");
        return QString();
    }
    if (info.size() > LinkUpload::MaxBytes) {
        m_lastError = tr("Files sent as a link can be up to 1 GB.");
        return QString();
    }
    Item it;
    it.id = QStringLiteral("link-%1-%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(m_nextLink++);
    it.contact = uin;
    it.incoming = false;
    it.fileName = info.fileName();
    it.size = info.size();
    it.path = info.absoluteFilePath();
    it.method = Link;
    it.hours = hours;
    it.state = Uploading;

    StoredMessage m;
    m.contact = uin;
    m.outgoing = true;
    m.kind = StoredMessage::File;
    m.fileName = it.fileName;
    m.fileSize = it.size;
    m.filePath = it.path;
    m.fileState = Uploading;
    m.transferId = it.id;
    m.timestamp = QDateTime::currentDateTime();
    it.messageId = m_account->addFileMessage(m);

    add(it);
    startUpload(it.id);
    m_lastError.clear();
    return it.id;
}

void TransferManager::startUpload(const QString &id)
{
    Item *it = find(id);
    if (!it)
        return;
    LinkUpload *upload = new LinkUpload(this);
    it->upload = upload;
    connect(upload, &LinkUpload::progress, this, [this, id](qint64 sent, qint64) { setDone(id, sent); });
    connect(upload, &LinkUpload::failed, this, [this, id, upload](const QString &why) {
        upload->deleteLater();
        Item *it = find(id);
        if (it && it->state == Uploading)
            setState(id, Failed, why);
    });
    connect(upload, &LinkUpload::finished, this, [this, id, upload](const QString &url) {
        upload->deleteLater();
        Item *it = find(id);
        if (!it || it->state != Uploading)
            return;
        it->url = url;
        const QString text = tr("%1 (%2): %3 (link works for %n hour(s))", nullptr, it->hours)
                .arg(it->fileName, formatSize(it->size), url);
        if (m_account->sendText(it->contact, text, false))
            setState(id, Done);
        else
            setState(id, Failed, tr("Uploaded, but you were offline, so the link was not sent. Copy it from here."));
    });
    upload->start(it->path, it->hours);
}

QString TransferManager::retryAsLink(const QString &id, int hours)
{
    Item *it = find(id);
    if (!it || it->incoming || isActive(it->state) || it->path.isEmpty())
        return QString();
    return sendLink(it->contact, it->path, hours);
}

int TransferManager::stateOf(const QString &id) const
{
    const int row = rowOf(id);
    return row < 0 ? -1 : m_items.at(row).state;
}

QString TransferManager::errorOf(const QString &id) const
{
    const int row = rowOf(id);
    return row < 0 ? QString() : m_items.at(row).error;
}
