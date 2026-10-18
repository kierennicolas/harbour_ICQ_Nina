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
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, { DeliveredRole });
            return;
        }
    }
}
