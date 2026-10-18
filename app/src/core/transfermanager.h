// File transfers for one account: OSCAR file transfer (direct, or through
// the server's rendezvous proxy) in both directions, and "send as a link"
// (upload to a temporary file host, then send the link as a message).
// Each transfer also appears in the conversation as a file message.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef TRANSFERMANAGER_H
#define TRANSFERMANAGER_H

#include <QAbstractListModel>
#include <QPointer>
#include <QVector>

class AccountManager;
class FileTransferHandler;
class LinkUpload;
namespace Oscar { class Client; }

class TransferManager : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int activeCount READ activeCount NOTIFY activeCountChanged)
    Q_PROPERTY(QString downloadDir READ downloadDir CONSTANT)

public:
    // Stored in the history too, so keep the numbers stable.
    enum State {
        Offered = 0,    // incoming, waiting for you to accept
        Waiting,        // waiting for the other side / connecting
        Transferring,
        Done,
        Failed,
        Cancelled,
        Declined,
        Uploading       // sending as a link: uploading to the file host
    };
    Q_ENUM(State)

    enum Method { Direct = 0, Link };
    Q_ENUM(Method)

    enum Roles {
        IdRole = Qt::UserRole + 1,
        ContactRole,
        IncomingRole,
        FileNameRole,
        FileSizeRole,
        DoneRole,
        StateRole,
        PathRole,
        UrlRole,
        ErrorRole,
        MethodRole
    };

    explicit TransferManager(AccountManager *account, const QString &downloadDir);
    ~TransferManager() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int activeCount() const;
    QString downloadDir() const { return m_downloadDir; }

    // The account's connection (nullptr when offline: running direct
    // transfers then fail; uploads carry on).
    void setClient(Oscar::Client *client);
    void handleIncoming(FileTransferHandler *handler);

    // pathOrUrl: a local path or file:// URL. Return the transfer id, or an
    // empty string (then lastError says why).
    Q_INVOKABLE QString sendFile(const QString &uin, const QString &pathOrUrl);
    Q_INVOKABLE QString sendLink(const QString &uin, const QString &pathOrUrl, int hours = 72);
    Q_INVOKABLE bool accept(const QString &id);
    Q_INVOKABLE void decline(const QString &id);
    Q_INVOKABLE void cancel(const QString &id);
    // Re-send a failed direct transfer as a link.
    Q_INVOKABLE QString retryAsLink(const QString &id, int hours = 72);

    Q_INVOKABLE int stateOf(const QString &id) const;
    Q_INVOKABLE QString errorOf(const QString &id) const;
    Q_INVOKABLE QString lastError() const { return m_lastError; }
    Q_INVOKABLE static QString formatSize(qint64 bytes);

    // The file name a received file gets in dir (no overwriting).
    static QString uniquePath(const QString &dir, const QString &fileName);

signals:
    void activeCountChanged();
    void incomingOffer(const QString &id, const QString &uin, const QString &fileName, qint64 size);
    void transferEnded(const QString &id, const QString &uin, const QString &fileName, bool incoming, int state);

private:
    struct Item {
        QString id;
        QString contact;
        bool incoming = false;
        QString fileName;
        int fileCount = 1;
        qint64 size = 0;
        qint64 done = 0;
        int state = Waiting;
        QString path;
        QString url;
        QString error;
        int method = Direct;
        qint64 messageId = 0;
        int hours = 72;
        QPointer<FileTransferHandler> handler;
        QPointer<LinkUpload> upload;
    };

    int rowOf(const QString &id) const;
    Item *find(const QString &id);
    void add(Item item);
    void changed(int row);
    void setState(const QString &id, int state, const QString &error = QString());
    void setDone(const QString &id, qint64 done);
    void watch(FileTransferHandler *handler, const QString &id);
    void startUpload(const QString &id);
    static bool isActive(int state);
    static QString localPath(const QString &pathOrUrl);

    AccountManager *m_account;
    QString m_downloadDir;
    QPointer<Oscar::Client> m_client;
    QVector<Item> m_items;
    QString m_lastError;
    int m_nextLink = 1;
};

#endif
