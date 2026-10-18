// Message history and contact cache in SQLite (QtSql), kept in the app's
// data folder. Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef HISTORYSTORE_H
#define HISTORYSTORE_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct StoredMessage
{
    qint64 id = 0;
    QString contact;
    bool outgoing = false;
    QString text;
    QDateTime timestamp;
    bool delivered = false;
    bool read = true;
};

struct StoredContact
{
    QString uin;
    QString alias;
    QString group;
};

class HistoryStore
{
public:
    // path: database file; account: our own number (history is per account)
    bool open(const QString &path, const QString &account);
    void close();
    bool isOpen() const;

    qint64 addMessage(const StoredMessage &m);
    void setDelivered(qint64 id);
    void markRead(const QString &contact);
    QVector<StoredMessage> messages(const QString &contact, int limit = 300) const;
    int unreadCount(const QString &contact) const;
    // contact -> (last message text, time) for everyone we have talked to
    QVector<StoredMessage> lastMessages() const;

    void saveContacts(const QVector<StoredContact> &contacts);
    QVector<StoredContact> contacts() const;

private:
    QSqlDatabase m_db;
    QString m_account;
    QString m_connection;
};

#endif
