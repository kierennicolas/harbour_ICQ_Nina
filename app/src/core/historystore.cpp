// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "historystore.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>
#include <QDebug>

namespace {
const char kColumns[] = "id, contact, outgoing, text, ts, delivered, read, kind, file_name, file_size,"
                        " file_path, file_url, file_state, transfer_id, media_type, media_info, queued";

StoredMessage fromRow(const QSqlQuery &q)
{
    StoredMessage m;
    m.id = q.value(0).toLongLong();
    m.contact = q.value(1).toString();
    m.outgoing = q.value(2).toInt() != 0;
    m.text = q.value(3).toString();
    m.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(4).toLongLong());
    m.delivered = q.value(5).toInt() != 0;
    m.read = q.value(6).toInt() != 0;
    m.kind = q.value(7).toInt();
    m.fileName = q.value(8).toString();
    m.fileSize = q.value(9).toLongLong();
    m.filePath = q.value(10).toString();
    m.fileUrl = q.value(11).toString();
    m.fileState = q.value(12).toInt();
    m.transferId = q.value(13).toString();
    m.mediaType = q.value(14).toInt();
    m.mediaInfo = q.value(15).toString();
    m.queued = q.value(16).toInt() != 0;
    return m;
}
} // namespace

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
        "CREATE TABLE IF NOT EXISTS hidden ("
        " account TEXT NOT NULL, contact TEXT NOT NULL, since INTEGER NOT NULL,"
        " PRIMARY KEY(account, contact))",
    };
    for (const char *sql : schema) {
        if (!q.exec(QString::fromLatin1(sql))) {
            qWarning() << "history: schema" << q.lastError().text();
            return false;
        }
    }
    return migrate();
}

// Version 0.2 added file messages.
bool HistoryStore::migrate()
{
    QSqlQuery q(m_db);
    QStringList columns;
    if (q.exec(QStringLiteral("PRAGMA table_info(messages)")))
        while (q.next())
            columns << q.value(1).toString();
    const QList<QPair<QString, QString>> wanted = {
        { QStringLiteral("kind"), QStringLiteral("INTEGER NOT NULL DEFAULT 0") },
        { QStringLiteral("file_name"), QStringLiteral("TEXT") },
        { QStringLiteral("file_size"), QStringLiteral("INTEGER NOT NULL DEFAULT 0") },
        { QStringLiteral("file_path"), QStringLiteral("TEXT") },
        { QStringLiteral("file_url"), QStringLiteral("TEXT") },
        { QStringLiteral("file_state"), QStringLiteral("INTEGER NOT NULL DEFAULT 0") },
        { QStringLiteral("transfer_id"), QStringLiteral("TEXT") },
        // 0.3: photos, voice messages, stickers.
        { QStringLiteral("media_type"), QStringLiteral("INTEGER NOT NULL DEFAULT 0") },
        { QStringLiteral("media_info"), QStringLiteral("TEXT") },
        // 0.3.4: the outbox.
        { QStringLiteral("queued"), QStringLiteral("INTEGER NOT NULL DEFAULT 0") },
    };
    for (const auto &c : wanted) {
        if (columns.contains(c.first))
            continue;
        if (!q.exec(QStringLiteral("ALTER TABLE messages ADD COLUMN %1 %2").arg(c.first, c.second))) {
            qWarning() << "history: migrate" << q.lastError().text();
            return false;
        }
    }
    // Transfers cannot survive a restart: whatever was still running failed.
    // (States: 0 offered, 1 waiting, 2 transferring, 7 uploading -> 4 failed.)
    q.exec(QStringLiteral("UPDATE messages SET file_state = 4 WHERE kind = 1 AND file_state IN (0, 1, 2, 7)"));
    // Media: an interrupted upload failed (can be retried), an interrupted
    // download is simply not loaded yet.
    q.exec(QStringLiteral("UPDATE messages SET file_state = 4 WHERE kind = 2 AND file_state = 7"));
    q.exec(QStringLiteral("UPDATE messages SET file_state = 0 WHERE kind = 2 AND file_state IN (1, 2)"));
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
    q.prepare(QStringLiteral("INSERT INTO messages (account, contact, outgoing, text, ts, delivered, read,"
                             " kind, file_name, file_size, file_path, file_url, file_state, transfer_id,"
                             " media_type, media_info, queued)"
                             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(m_account);
    q.addBindValue(m.contact);
    q.addBindValue(m.outgoing ? 1 : 0);
    q.addBindValue(m.text);
    q.addBindValue(m.timestamp.toMSecsSinceEpoch());
    q.addBindValue(m.delivered ? 1 : 0);
    q.addBindValue(m.read ? 1 : 0);
    q.addBindValue(m.kind);
    q.addBindValue(m.fileName);
    q.addBindValue(m.fileSize);
    q.addBindValue(m.filePath);
    q.addBindValue(m.fileUrl);
    q.addBindValue(m.fileState);
    q.addBindValue(m.transferId);
    q.addBindValue(m.mediaType);
    q.addBindValue(m.mediaInfo);
    q.addBindValue(m.queued ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "history: insert" << q.lastError().text();
        return 0;
    }
    const qint64 id = q.lastInsertId().toLongLong();
    QSqlQuery unhide(m_db);
    unhide.prepare(QStringLiteral("DELETE FROM hidden WHERE account = ? AND contact = ?"));
    unhide.addBindValue(m_account);
    unhide.addBindValue(m.contact);
    unhide.exec();
    return id;
}

void HistoryStore::setDelivered(qint64 id)
{
    if (!isOpen() || id <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE messages SET delivered = 1, queued = 0 WHERE id = ?"));
    q.addBindValue(id);
    q.exec();
}

void HistoryStore::setQueued(qint64 id, bool queued)
{
    if (!isOpen() || id <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE messages SET queued = ? WHERE id = ?"));
    q.addBindValue(queued ? 1 : 0);
    q.addBindValue(id);
    q.exec();
}

QVector<StoredMessage> HistoryStore::queuedMessages(const QString &contact) const
{
    QVector<StoredMessage> out;
    if (!isOpen())
        return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM messages WHERE account = ? AND outgoing = 1 AND queued = 1%2"
                             " ORDER BY ts, id").arg(QLatin1String(kColumns),
                                                     contact.isEmpty() ? QString() : QStringLiteral(" AND contact = ?")));
    q.addBindValue(m_account);
    if (!contact.isEmpty())
        q.addBindValue(contact);
    if (q.exec())
        while (q.next())
            out.append(fromRow(q));
    return out;
}

void HistoryStore::deleteMessage(qint64 id)
{
    if (!isOpen() || id <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM messages WHERE account = ? AND id = ?"));
    q.addBindValue(m_account);
    q.addBindValue(id);
    q.exec();
}

void HistoryStore::updateFile(qint64 id, int state, const QString &path, const QString &url, qint64 size)
{
    if (!isOpen() || id <= 0)
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE messages SET file_state = ?, file_path = ?, file_url = ?,"
                             " file_size = CASE WHEN ? < 0 THEN file_size ELSE ? END WHERE id = ?"));
    q.addBindValue(state);
    q.addBindValue(path);
    q.addBindValue(url);
    q.addBindValue(size);
    q.addBindValue(size);
    q.addBindValue(id);
    q.exec();
}

bool HistoryStore::hasIncoming(const QString &contact, const QString &text, const QDateTime &when, int toleranceSecs) const
{
    if (!isOpen())
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT 1 FROM messages WHERE account = ? AND contact = ? AND outgoing = 0"
                             " AND text = ? AND ts BETWEEN ? AND ? LIMIT 1"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    q.addBindValue(text);
    q.addBindValue(when.toMSecsSinceEpoch() - qint64(toleranceSecs) * 1000);
    q.addBindValue(when.toMSecsSinceEpoch() + qint64(toleranceSecs) * 1000);
    return q.exec() && q.next();
}

void HistoryStore::hideConversation(const QString &contact)
{
    if (!isOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO hidden (account, contact, since) VALUES (?, ?, ?)"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    q.exec();
}

void HistoryStore::deleteConversation(const QString &contact)
{
    if (!isOpen())
        return;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM messages WHERE account = ? AND contact = ?"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
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
    q.prepare(QStringLiteral("SELECT %1 FROM messages"
                             " WHERE account = ? AND contact = ? ORDER BY ts DESC, id DESC LIMIT ?").arg(QLatin1String(kColumns)));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next())
        out.prepend(fromRow(q)); // oldest first
    return out;
}

StoredMessage HistoryStore::message(qint64 id) const
{
    if (!isOpen() || id <= 0)
        return StoredMessage();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT %1 FROM messages WHERE account = ? AND id = ?").arg(QLatin1String(kColumns)));
    q.addBindValue(m_account);
    q.addBindValue(id);
    return q.exec() && q.next() ? fromRow(q) : StoredMessage();
}

QStringList HistoryStore::mediaFiles(const QString &contact) const
{
    QStringList out;
    if (!isOpen())
        return out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT file_path FROM messages WHERE account = ? AND contact = ? AND kind = 2"
                             " AND file_path IS NOT NULL AND file_path != ''"));
    q.addBindValue(m_account);
    q.addBindValue(contact);
    if (q.exec())
        while (q.next())
            out << q.value(0).toString();
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
    q.prepare(QStringLiteral("SELECT m.contact, m.outgoing, m.text, m.ts, m.kind, m.media_type, m.media_info FROM messages m"
                             " JOIN (SELECT contact, MAX(id) AS last FROM messages WHERE account = ? GROUP BY contact) l"
                             " ON m.id = l.last"
                             " LEFT JOIN hidden h ON h.account = ? AND h.contact = m.contact"
                             " WHERE h.contact IS NULL"));
    q.addBindValue(m_account);
    q.addBindValue(m_account);
    if (!q.exec())
        return out;
    while (q.next()) {
        StoredMessage m;
        m.contact = q.value(0).toString();
        m.outgoing = q.value(1).toInt() != 0;
        m.text = q.value(2).toString();
        m.timestamp = QDateTime::fromMSecsSinceEpoch(q.value(3).toLongLong());
        m.kind = q.value(4).toInt();
        m.mediaType = q.value(5).toInt();
        m.mediaInfo = q.value(6).toString();
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
