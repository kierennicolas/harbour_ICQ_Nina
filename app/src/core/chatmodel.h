// The messages of one conversation, for a Silica ListView.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef CHATMODEL_H
#define CHATMODEL_H

#include <QAbstractListModel>
#include <QVector>

#include "historystore.h"

class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString contact READ contact CONSTANT)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        OutgoingRole,
        TimestampRole,
        DeliveredRole,
        DayRole,          // "yyyy-MM-dd", for section headers
        IdRole
    };

    ChatModel(const QString &contact, HistoryStore *store, QObject *parent = nullptr);

    QString contact() const { return m_contact; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds a message (already stored) at the end.
    void append(const StoredMessage &m);
    void setDelivered(qint64 id);
    void reload();

signals:
    void countChanged();

private:
    QString m_contact;
    HistoryStore *m_store;
    QVector<StoredMessage> m_messages;
};

#endif
