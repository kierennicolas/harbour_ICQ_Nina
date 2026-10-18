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
    enum Kind { Text = 0, File = 1 };

    qint64 id = 0;
    QString contact;
    bool outgoing = false;
    QString text;          // for files: a one-line summary for the contact list
    QDateTime timestamp;
    bool delivered = false;
    bool read = true;

    int kind = Text;
    // Files (kind == File):
    QString fileName;
    qint64 fileSize = 0;
    QString filePath;      // the local file (sent, or saved when received)
    QString fileUrl;       // download link when sent as a link
    int fileState = 0;     // TransferModel::State
    QString transferId;
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
    void updateFile(qint64 id, int state, const QString &path, const QString &url);
    // Whether an incoming message with this text from this contact was
    // stored within toleranceSecs of when (offline messages can arrive twice).
    bool hasIncoming(const QString &contact, const QString &text, const QDateTime &when, int toleranceSecs) const;
    void markRead(const QString &contact);
    void deleteConversation(const QString &contact);
    QVector<StoredMessage> messages(const QString &contact, int limit = 300) const;
    int unreadCount(const QString &contact) const;
    // contact -> (last message text, time) for everyone we have talked to
    QVector<StoredMessage> lastMessages() const;

    void saveContacts(const QVector<StoredContact> &contacts);
    QVector<StoredContact> contacts() const;

private:
    bool migrate();
    QSqlDatabase m_db;
    QString m_account;
    QString m_connection;
};

#endif
