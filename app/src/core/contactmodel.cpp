// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "contactmodel.h"

#include <algorithm>

#include "oscarutils.h"

ContactModel::ContactModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ContactModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ContactModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();
    const Item &c = m_items.at(index.row());
    switch (role) {
    case UinRole: return c.uin;
    case NameRole: return c.alias.isEmpty() ? c.uin : c.alias;
    case GroupRole: return c.group;
    case PresenceRole: return c.presence;
    case OnlineRole: return c.presence != Offline;
    case StatusMessageRole: return c.statusMessage;
    case UnreadRole: return c.unread;
    case LastMessageRole: return c.lastMessage;
    case LastMessageTimeRole: return c.lastMessageTime;
    case AwaitingAuthRole: return c.awaitingAuth;
    case TypingRole: return c.typing;
    }
    return QVariant();
}

QHash<int, QByteArray> ContactModel::roleNames() const
{
    return {
        { UinRole, "uin" },
        { NameRole, "name" },
        { GroupRole, "group" },
        { PresenceRole, "presence" },
        { OnlineRole, "online" },
        { StatusMessageRole, "statusMessage" },
        { UnreadRole, "unread" },
        { LastMessageRole, "lastMessage" },
        { LastMessageTimeRole, "lastMessageTime" },
        { AwaitingAuthRole, "awaitingAuth" },
        { TypingRole, "typing" },
    };
}

int ContactModel::indexOf(const QString &uin) const
{
    const QString n = Oscar::normalize(uin);
    for (int i = 0; i < m_items.size(); ++i)
        if (Oscar::normalize(m_items.at(i).uin) == n)
            return i;
    return -1;
}

void ContactModel::sortAndReset()
{
    // Groups alphabetically; inside a group, people we are chatting with
    // (unread first), then online before offline, then by name.
    std::stable_sort(m_items.begin(), m_items.end(), [](const Item &a, const Item &b) {
        const int g = QString::localeAwareCompare(a.group, b.group);
        if (g != 0) return g < 0;
        if ((a.unread > 0) != (b.unread > 0)) return a.unread > 0;
        const bool ao = a.presence != Offline, bo = b.presence != Offline;
        if (ao != bo) return ao;
        const QString an = a.alias.isEmpty() ? a.uin : a.alias;
        const QString bn = b.alias.isEmpty() ? b.uin : b.alias;
        return QString::localeAwareCompare(an, bn) < 0;
    });
}

void ContactModel::setContacts(const QVector<Item> &items)
{
    beginResetModel();
    QVector<Item> merged;
    merged.reserve(items.size());
    for (const Item &in : items) {
        Item c = in;
        const int old = indexOf(in.uin);
        if (old >= 0) {
            const Item &o = m_items.at(old);
            c.presence = o.presence;
            c.statusMessage = o.statusMessage;
            c.unread = qMax(c.unread, o.unread);
            if (c.lastMessage.isEmpty()) {
                c.lastMessage = o.lastMessage;
                c.lastMessageTime = o.lastMessageTime;
            }
        }
        merged.append(c);
    }
    // People who wrote to us but are not on the list stay visible.
    for (const Item &o : m_items) {
        bool found = false;
        for (const Item &c : merged)
            if (Oscar::normalize(c.uin) == Oscar::normalize(o.uin)) { found = true; break; }
        if (!found && (o.unread > 0 || !o.lastMessage.isEmpty())) {
            Item keep = o;
            keep.group = QString();
            merged.append(keep);
        }
    }
    m_items = merged;
    sortAndReset();
    endResetModel();
    emit countChanged();
    emit presenceChanged();
    emit unreadChanged();
}

void ContactModel::upsert(const Item &item)
{
    const int i = indexOf(item.uin);
    if (i < 0) {
        beginResetModel();
        m_items.append(item);
        sortAndReset();
        endResetModel();
        emit countChanged();
        return;
    }
    Item &c = m_items[i];
    if (c.alias == item.alias && c.group == item.group && c.awaitingAuth == item.awaitingAuth)
        return;
    c.alias = item.alias;
    c.group = item.group;
    c.awaitingAuth = item.awaitingAuth;
    beginResetModel();
    sortAndReset();
    endResetModel();
}

void ContactModel::remove(const QString &uin)
{
    const int i = indexOf(uin);
    if (i < 0)
        return;
    beginRemoveRows(QModelIndex(), i, i);
    m_items.remove(i);
    endRemoveRows();
    emit countChanged();
    emit unreadChanged();
}

void ContactModel::changed(int row, const QVector<int> &roles)
{
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, roles);
}

void ContactModel::setPresence(const QString &uin, int presence, const QString &statusMessage)
{
    const int i = indexOf(uin);
    if (i < 0)
        return;
    Item &c = m_items[i];
    const bool onlineChanged = (c.presence == Offline) != (presence == Offline);
    c.presence = presence;
    if (!statusMessage.isNull() || presence == Offline)
        c.statusMessage = statusMessage;
    if (onlineChanged) {
        beginResetModel();
        sortAndReset();
        endResetModel();
    } else {
        changed(i, { PresenceRole, OnlineRole, StatusMessageRole });
    }
    emit presenceChanged();
}

void ContactModel::setAllOffline()
{
    beginResetModel();
    for (Item &c : m_items) {
        c.presence = Offline;
        c.typing = false;
    }
    sortAndReset();
    endResetModel();
    emit presenceChanged();
}

void ContactModel::setUnread(const QString &uin, int unread)
{
    int i = indexOf(uin);
    if (i < 0) {
        Item c;
        c.uin = uin;
        c.unread = unread;
        upsert(c);
        i = indexOf(uin);
    }
    if (i < 0 || m_items[i].unread == unread)
        return;
    const bool moved = (m_items[i].unread > 0) != (unread > 0);
    m_items[i].unread = unread;
    if (moved) {
        beginResetModel();
        sortAndReset();
        endResetModel();
    } else {
        changed(i, { UnreadRole });
    }
    emit unreadChanged();
}

void ContactModel::setLastMessage(const QString &uin, const QString &text, const QDateTime &when)
{
    int i = indexOf(uin);
    if (i < 0) {
        Item c;
        c.uin = uin;
        upsert(c);
        i = indexOf(uin);
    }
    if (i < 0)
        return;
    m_items[i].lastMessage = text;
    m_items[i].lastMessageTime = when;
    changed(i, { LastMessageRole, LastMessageTimeRole });
}

void ContactModel::setTyping(const QString &uin, bool typing)
{
    const int i = indexOf(uin);
    if (i < 0 || m_items[i].typing == typing)
        return;
    m_items[i].typing = typing;
    changed(i, { TypingRole });
}

QString ContactModel::displayName(const QString &uin) const
{
    const int i = indexOf(uin);
    if (i < 0 || m_items.at(i).alias.isEmpty())
        return uin;
    return m_items.at(i).alias;
}

int ContactModel::presenceOf(const QString &uin) const
{
    const int i = indexOf(uin);
    return i < 0 ? int(Offline) : m_items.at(i).presence;
}

bool ContactModel::contains(const QString &uin) const
{
    return indexOf(uin) >= 0;
}

bool ContactModel::isAwaitingAuth(const QString &uin) const
{
    const int i = indexOf(uin);
    return i >= 0 && m_items.at(i).awaitingAuth;
}

bool ContactModel::isTyping(const QString &uin) const
{
    const int i = indexOf(uin);
    return i >= 0 && m_items.at(i).typing;
}

int ContactModel::onlineCount() const
{
    int n = 0;
    for (const Item &c : m_items)
        if (c.presence != Offline) ++n;
    return n;
}

int ContactModel::unreadTotal() const
{
    int n = 0;
    for (const Item &c : m_items)
        n += c.unread;
    return n;
}
