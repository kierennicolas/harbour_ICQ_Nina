// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "searchmodel.h"

QString SearchModel::displayName(const Item &item)
{
    if (!item.nickname.trimmed().isEmpty())
        return item.nickname.trimmed();
    const QString full = (item.firstName + QLatin1Char(' ') + item.lastName).trimmed();
    return full.isEmpty() ? item.uin : full;
}

QVariant SearchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return QVariant();
    const Item &it = m_items.at(index.row());
    switch (role) {
    case UinRole: return it.uin;
    case NameRole: return displayName(it);
    case NicknameRole: return it.nickname;
    case FirstNameRole: return it.firstName;
    case LastNameRole: return it.lastName;
    case EmailRole: return it.email;
    case OnlineRole: return it.online;
    case AuthRequiredRole: return it.authRequired;
    case GenderRole: return it.gender;
    case AgeRole: return it.age;
    }
    return QVariant();
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        { UinRole, "uin" }, { NameRole, "name" }, { NicknameRole, "nickname" },
        { FirstNameRole, "firstName" }, { LastNameRole, "lastName" }, { EmailRole, "email" },
        { OnlineRole, "online" }, { AuthRequiredRole, "authRequired" }, { GenderRole, "gender" },
        { AgeRole, "age" },
    };
}

void SearchModel::clear()
{
    if (m_items.isEmpty())
        return;
    beginResetModel();
    m_items.clear();
    endResetModel();
    emit countChanged();
}

void SearchModel::add(const Item &item)
{
    for (const Item &it : m_items)
        if (it.uin == item.uin)
            return;
    beginInsertRows(QModelIndex(), m_items.size(), m_items.size());
    m_items.append(item);
    endInsertRows();
    emit countChanged();
}
