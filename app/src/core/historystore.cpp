// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "historystore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

bool HistoryStore::open(const QString &path, const QString &account)
{
    close();
    m_account = account;
    m_connection = QStringLiteral("history-%1").arg(reinterpret_cast<quintptr>(this));
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connection);
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "history: cannot open" << path << m_db.lastError().text();
        return false;
    }
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    const char *schema[] = {
        "CREATE TABLE IF NOT EXISTS messages ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT, account TEXT NOT NULL, contact TEXT NOT NULL,"
        " outgoing INTEGER NOT NULL, text TEXT NOT NULL, ts INTEGER NOT NULL,"
        " delivered INTEGER NOT NULL DEFAULT 0, read INTEGER NOT NULL DEFAULT 1)",
        "CREATE INDEX IF NOT EXISTS messages_by_contact ON messages(account, contact, ts)",
        "CREATE TABLE IF NOT EXISTS contacts ("
        " account TEXT NOT NULL, uin TEXT NOT NULL, alias TEXT, grp TEXT,"
        " PRIMARY KEY(account, uin))",
    };
    for (const char *sql : schema) {
        if (!q.exec(QString::fromLatin1(sql))) {
            qWarning() << "history: schema" << q.lastError().text();
            return false;
        }
    }
    return true;
}

void HistoryStore::close()
{
    if (m_db.isValid()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connection);
    }
}

bool HistoryStore::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

qint64 HistoryStore::addMessage(const StoredMessage &m)
{
    if (!isOpen())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO messages (account, contact, outgoing, text, ts, delivered, read)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_account);
    q.addBindValue(m.contact);
    q.addBindValue(m.outgoing ? 1 : 0);
    q.addBindValue(m.text);
    q.addBindValue(m.timestamp.toMSecsSinceEpoch());
    q.addBindValue(m.delivered ? 1 : 0);
    q.addBindValue(m.read ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "history: insert" << q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

void HistoryStore::setDelivered(qint64 id)
{
    if (!isOpen() || id <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE messages SET delivered = 1 WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

void HistoryStore::markRead(const QString &contact)
{
    if (!isOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE messages SET read = 1 WHERE account = ? AND contact = ? AND read = 0"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    q.exec();
}

QVector<StoredMessage> HistoryStore::messages(const QString &contact, int limit) const
{
    QVector<StoredMessage> out;
    if (!isOpen())
        return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, outgoing, text, ts, delivered, read FROM messages"
                             " WHERE account = ? AND contact = ? ORDER BY ts DESC, id DESC LIMIT ?"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        StoredMessage m;
        m.id = q.value(0).toLongLong();
        m.contact = contact;
        m.outgoing = q.value(1).toInt() != 0;
        m.text = q.value(2).toString();
        m.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        m.delivered = q.value(4).toInt() != 0;
        m.read = q.value(5).toInt() != 0;
        out.prepend(m); // oldest first
    }
    return out;
}

int HistoryStore::unreadCount(const QString &contact) const
{
    if (!isOpen())
        return 0;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM messages WHERE account = ? AND contact = ? AND read = 0"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

QVector<StoredMessage> HistoryStore::lastMessages() const
{
    QVector<StoredMessage> out;
    if (!isOpen())
        return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT m.contact, m.outgoing, m.text, m.ts FROM messages m"
                             " JOIN (SELECT contact, MAX(id) AS last FROM messages WHERE account = ? GROUP BY contact) l"
                             " ON m.id = l.last"));
    q.addBindValue(m_account);
    if (!q.exec())
        return out;
    while (q.next()) {
        StoredMessage m;
        m.contact = q.value(0).toString();
        m.outgoing = q.value(1).toInt() != 0;
        m.text = q.value(2).toString();
        m.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        out.append(m);
    }
    return out;
}

void HistoryStore::saveContacts(const QVector<StoredContact> &contacts)
{
    if (!isOpen())
        return;
    m_db.transaction();
    QSqlQuery del(m_db);
    del.prepare(QStringLiteral("DELETE FROM contacts WHERE account = ?"));
    del.addBindValue(m_account);
    del.exec();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO contacts (account, uin, alias, grp) VALUES (?, ?, ?, ?)"));
    for (const StoredContact &c : contacts) {
        q.addBindValue(m_account);
        q.addBindValue(c.uin);
        q.addBindValue(c.alias);
        q.addBindValue(c.group);
        q.exec();
    }
    m_db.commit();
}

QVector<StoredContact> HistoryStore::contacts() const
{
    QVector<StoredContact> out;
    if (!isOpen())
        return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT uin, alias, grp FROM contacts WHERE account = ?"));
    q.addBindValue(m_account);
    if (!q.exec())
        return out;
    while (q.next())
        out.append({ q.value(0).toString(), q.value(1).toString(), q.value(2).toString() });
    return out;
}
