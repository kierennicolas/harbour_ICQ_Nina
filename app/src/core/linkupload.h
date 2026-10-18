// Uploads one file to Litterbox (litterbox.catbox.moe), a temporary file
// host, and reports the download link. Used for "send as a link", which works
// with any ICQ/AIM client and when the other person is offline.
//
// Adapted from LinkDrop's uploader (MIT, Copyright (c) 2026 LinkDrop
// contributors). Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef LINKUPLOAD_H
#define LINKUPLOAD_H

#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>

class QNetworkReply;

class LinkUpload : public QObject
{
    Q_OBJECT
public:
    static const qint64 MaxBytes = 1000LL * 1000 * 1000; // Litterbox's limit

    explicit LinkUpload(QObject *parent = nullptr);

    // hours: 1, 12, 24 or 72 (how long the link works).
    void start(const QString &path, int hours);
    void cancel();

signals:
    void progress(qint64 sent, qint64 total);
    void finished(const QString &url);
    void failed(const QString &message);

private:
    void done();

    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QTimer m_watchdog;
    bool m_cancelled = false;
    bool m_timedOut = false;
    bool m_waitingForServer = false;
};

#endif
