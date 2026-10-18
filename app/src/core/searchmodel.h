// Results of an ICQ directory search. Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef SEARCHMODEL_H
#define SEARCHMODEL_H

#include <QAbstractListModel>
#include <QVector>

class SearchModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Roles {
        UinRole = Qt::UserRole + 1,
        NameRole,          // best display name
        NicknameRole,
        FirstNameRole,
        LastNameRole,
        EmailRole,
        OnlineRole,
        AuthRequiredRole,
        GenderRole,        // 0 unknown, 1 female, 2 male
        AgeRole
    };

    struct Item {
        QString uin;
        QString nickname;
        QString firstName;
        QString lastName;
        QString email;
        bool online = false;
        bool authRequired = false;
        int gender = 0;
        int age = 0;
    };

    explicit SearchModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    { return parent.isValid() ? 0 : m_items.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void add(const Item &item);   // ignores duplicates

    static QString displayName(const Item &item);

signals:
    void countChanged();

private:
    QVector<Item> m_items;
};

#endif
