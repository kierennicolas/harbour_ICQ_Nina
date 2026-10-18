// The contact list: server-side contacts, presence, unread counts and the
// last message, grouped for a Silica ListView. Part of harbour_icq_nina;
// GPL-3.0-or-later.
#ifndef CONTACTMODEL_H
#define CONTACTMODEL_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QVector>

class ContactModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY presenceChanged)

public:
    // Presence of a contact (matches AccountManager::Status where it overlaps).
    enum Presence {
        Online = 0,
        Away,
        NotAvailable,
        Occupied,
        DoNotDisturb,
        FreeForChat,
        Invisible,
        Offline
    };
    Q_ENUM(Presence)

    enum Roles {
        UinRole = Qt::UserRole + 1,
        NameRole,
        GroupRole,
        PresenceRole,
        OnlineRole,
        StatusMessageRole,
        UnreadRole,
        LastMessageRole,
        LastMessageTimeRole,
        AwaitingAuthRole,
        TypingRole
    };

    struct Item {
        QString uin;
        QString alias;
        QString group;
        int presence = Offline;
        QString statusMessage;
        int unread = 0;
        QString lastMessage;
        QDateTime lastMessageTime;
        bool awaitingAuth = false;
        bool typing = false;
    };

    explicit ContactModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Replaces the whole list (from the server or the cache), keeping
    // presence, unread counts and last messages we already know.
    void setContacts(const QVector<Item> &items);
    void upsert(const Item &item);   // add or update uin/alias/group/awaitingAuth
    void remove(const QString &uin);
    void setPresence(const QString &uin, int presence, const QString &statusMessage = QString());
    void setAllOffline();
    void setUnread(const QString &uin, int unread);
    void setLastMessage(const QString &uin, const QString &text, const QDateTime &when);
    void setTyping(const QString &uin, bool typing);

    Q_INVOKABLE QString displayName(const QString &uin) const;
    Q_INVOKABLE int presenceOf(const QString &uin) const;
    Q_INVOKABLE bool contains(const QString &uin) const;
    Q_INVOKABLE bool isTyping(const QString &uin) const;
    Q_INVOKABLE bool isAwaitingAuth(const QString &uin) const;
    int onlineCount() const;
    int unreadTotal() const;
    QVector<Item> items() const { return m_items; }

signals:
    void countChanged();
    void presenceChanged();
    void unreadChanged();

private:
    int indexOf(const QString &uin) const;
    void sortAndReset();
    void changed(int row, const QVector<int> &roles);

    QVector<Item> m_items;
};

#endif
