// Adapted from LinkDrop's uploader (MIT, Copyright (c) 2026 LinkDrop
// contributors). Part of harbour_icq_nina; GPL-3.0-or-later.
#include "linkupload.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

#ifndef APP_VERSION
#define APP_VERSION "dev"
#endif

namespace {
const int kStallTimeoutMs = 90 * 1000;            // no bytes moving while sending
const int kServerReplyTimeoutMs = 5 * 60 * 1000;  // storing the file after the last byte

QUrl endpoint()
{
#ifdef ICQNINA_TESTING
    const QByteArray test = qgetenv("ICQNINA_TEST_UPLOAD_ENDPOINT");
    if (!test.isEmpty())
        return QUrl(QString::fromUtf8(test));
#endif
    return QUrl(QStringLiteral("https://litterbox.catbox.moe/resources/internals/api.php"));
}

QString expectedHost()
{
#ifdef ICQNINA_TESTING
    const QByteArray test = qgetenv("ICQNINA_TEST_UPLOAD_HOST");
    if (!test.isEmpty())
        return QString::fromUtf8(test);
#endif
    return QStringLiteral("litter.catbox.moe");
}

QString describeServerReply(const QByteArray &body)
{
    QString text = QString::fromUtf8(body).trimmed();
    if (text.isEmpty() || text.startsWith(QLatin1Char('<')))
        return QString();
    text.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return text.length() > 160 ? text.left(157) + QStringLiteral("...") : text;
}
} // namespace

LinkUpload::LinkUpload(QObject *parent)
    : QObject(parent)
{
    m_watchdog.setSingleShot(true);
    connect(&m_watchdog, &QTimer::timeout, this, [this]() {
        if (m_reply) {
            m_timedOut = true;
            m_reply->abort();
        }
    });
}

void LinkUpload::start(const QString &path, int hours)
{
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        emit failed(tr("Cannot read the file."));
        return;
    }
    if (info.size() > MaxBytes) {
        emit failed(tr("Files sent as a link can be up to 1 GB."));
        return;
    }
    if (hours != 1 && hours != 12 && hours != 24 && hours != 72)
        hours = 72;

    QFile *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        emit failed(tr("Cannot open the file."));
        return;
    }

    QHttpMultiPart *body = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    auto addField = [body](const QByteArray &name, const QByteArray &value) {
        QHttpPart part;
        part.setRawHeader("Content-Disposition", "form-data; name=\"" + name + "\"");
        part.setBody(value);
        body->append(part);
    };
    addField("reqtype", "fileupload");
    addField("time", QByteArray::number(hours) + "h");

    // The name goes into a quoted header value; keep it from breaking out.
    QString safeName = info.fileName();
    for (const QChar c : { QChar('"'), QChar('\\'), QChar('\r'), QChar('\n') })
        safeName.replace(c, QLatin1Char('_'));
    QHttpPart filePart;
    filePart.setRawHeader("Content-Disposition",
                          "form-data; name=\"fileToUpload\"; filename=\"" + safeName.toUtf8() + "\"");
    filePart.setRawHeader("Content-Type", "application/octet-stream");
    filePart.setBodyDevice(file);
    file->setParent(body);
    body->append(filePart);

    QNetworkRequest request(endpoint());
    request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, false);
    request.setRawHeader("User-Agent", "ICQNina-Sailfish/" APP_VERSION);

#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.6 bearer management can get stuck on "not accessible" after the
    // phone switches between Wi-Fi and mobile data.
    m_network.setNetworkAccessible(QNetworkAccessManager::Accessible);
#endif

    m_cancelled = m_timedOut = m_waitingForServer = false;
    QNetworkReply *reply = m_network.post(request, body);
    body->setParent(reply);
    m_reply = reply;
    m_watchdog.start(kStallTimeoutMs);

    connect(reply, &QNetworkReply::uploadProgress, this, [this, reply](qint64 sent, qint64 total) {
        if (m_reply != reply)
            return;
        if (total > 0) {
            emit progress(sent, total);
            if (sent >= total)
                m_waitingForServer = true;
        }
        m_watchdog.start(m_waitingForServer ? kServerReplyTimeoutMs : kStallTimeoutMs);
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (m_reply == reply)
            done();
        reply->deleteLater();
    });
}

void LinkUpload::cancel()
{
    if (m_reply) {
        m_cancelled = true;
        m_reply->abort();
    }
}

void LinkUpload::done()
{
    QNetworkReply *reply = m_reply.data();
    m_reply.clear();
    m_watchdog.stop();
    if (!reply)
        return;
    if (m_cancelled) {
        emit failed(tr("Cancelled."));
        return;
    }
    if (m_timedOut) {
        emit failed(tr("The upload stalled. Check the connection and try again."));
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();
    const QString serverSays = describeServerReply(body);
    if (status != 0 && (status < 200 || status >= 300)) {
        emit failed(tr("The file host refused the upload (HTTP %1). %2").arg(status).arg(serverSays).trimmed());
        return;
    }
    if (reply->error() != QNetworkReply::NoError) {
        emit failed(reply->errorString());
        return;
    }
    const QString text = QString::fromUtf8(body).trimmed();
    const QUrl url(text, QUrl::StrictMode);
    if (!url.isValid() || url.scheme() != QLatin1String("https")
            || url.host().compare(expectedHost(), Qt::CaseInsensitive) != 0
            || url.path().isEmpty() || url.path() == QLatin1String("/")
            || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
            || text.contains(QRegularExpression(QStringLiteral("\\s")))) {
        emit failed(tr("The file host did not return a download link. %1").arg(serverSays).trimmed());
        return;
    }
    emit finished(url.toString(QUrl::FullyEncoded));
}
