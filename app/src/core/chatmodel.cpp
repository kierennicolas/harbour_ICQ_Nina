// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "chatmodel.h"

ChatModel::ChatModel(const QString &contact, HistoryStore *store, QObject *parent)
    : QAbstractListModel(parent), m_contact(contact), m_store(store)
{
    reload();
}

void ChatModel::reload()
{
    beginResetModel();
    m_messages = m_store ? m_store->messages(m_contact) : QVector<StoredMessage>();
    endResetModel();
    emit countChanged();
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_messages.size();
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_messages.size())
        return QVariant();
    const StoredMessage &m = m_messages.at(index.row());
    switch (role) {
    case TextRole: return m.text;
    case OutgoingRole: return m.outgoing;
    case TimestampRole: return m.timestamp;
    case DeliveredRole: return m.delivered;
    case DayRole: return m.timestamp.toLocalTime().date().toString(Qt::ISODate);
    case IdRole: return m.id;
    case KindRole: return m.kind;
    case FileNameRole: return m.fileName;
    case FileSizeRole: return m.fileSize;
    case FilePathRole: return m.filePath;
    case FileUrlRole: return m.fileUrl;
    case FileStateRole: return m.fileState;
    case FileDoneRole: return m_fileDone.value(m.id, m.fileState == 3 ? m.fileSize : 0);
    case TransferIdRole: return m.transferId;
    case MediaTypeRole: return m.mediaType;
    case MediaInfoRole: return m.mediaInfo;
    case QueuedRole: return m.queued;
    }
    return QVariant();
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {
        { TextRole, "text" },
        { OutgoingRole, "outgoing" },
        { TimestampRole, "timestamp" },
        { DeliveredRole, "delivered" },
        { DayRole, "day" },
        { IdRole, "messageId" },
        { KindRole, "kind" },
        { FileNameRole, "fileName" },
        { FileSizeRole, "fileSize" },
        { FilePathRole, "filePath" },
        { FileUrlRole, "fileUrl" },
        { FileStateRole, "fileState" },
        { FileDoneRole, "fileDone" },
        { TransferIdRole, "transferId" },
        { MediaTypeRole, "mediaType" },
        { MediaInfoRole, "mediaInfo" },
        { QueuedRole, "queued" },
    };
}

void ChatModel::append(const StoredMessage &m)
{
    beginInsertRows(QModelIndex(), m_messages.size(), m_messages.size());
    m_messages.append(m);
    endInsertRows();
    emit countChanged();
}

void ChatModel::setDelivered(qint64 id)
{
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages[i].id == id) {
            m_messages[i].delivered = true;
            m_messages[i].queued = false;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, { DeliveredRole, QueuedRole });
            return;
        }
    }
}

void ChatModel::setQueued(qint64 id, bool queued)
{
    const int row = rowOf(id);
    if (row < 0 || m_messages[row].queued == queued)
        return;
    m_messages[row].queued = queued;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { QueuedRole });
}

void ChatModel::remove(qint64 id)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_messages.remove(row);
    endRemoveRows();
    emit countChanged();
}

int ChatModel::rowOf(qint64 id) const
{
    for (int i = m_messages.size() - 1; i >= 0; --i)
        if (m_messages.at(i).id == id)
            return i;
    return -1;
}

void ChatModel::updateFile(qint64 id, int state, const QString &path, const QString &url, qint64 size)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    StoredMessage &m = m_messages[row];
    m.fileState = state;
    m.filePath = path;
    m.fileUrl = url;
    if (size >= 0)
        m.fileSize = size;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { FileStateRole, FilePathRole, FileUrlRole, FileDoneRole, FileSizeRole });
}

void ChatModel::setFileDone(qint64 id, qint64 bytes)
{
    const int row = rowOf(id);
    if (row < 0)
        return;
    m_fileDone.insert(id, bytes);
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { FileDoneRole });
}
