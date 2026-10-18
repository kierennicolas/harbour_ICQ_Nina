// oscarprobe - sign on to an OSCAR (ICQ/AIM) server with liboscar and report
// what happens: login result, own info, server-side contact list, presence,
// incoming messages, typing, authorization requests and errors.
//
// The password is read from an environment variable (default OSCAR_PASSWORD)
// and is never printed. Part of harbour_icq_nina; GPL-3.0-or-later.

#include <QCommandLineParser>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QTextCodec>
#include <QTimer>

#include <cstdio>

#include "client.h"
#include "contactmanager.h"
#include "oscarmessage.h"
#include "oscartypes.h"
#include "oscarutils.h"
#include "userdetails.h"

namespace {

int g_exitCode = 0;
bool g_loggedIn = false;
bool g_expectedSeen = false;
bool g_redact = false;
QString g_self;

// With --redact (used in public CI logs) other people's numbers/names and
// message texts are masked; only counts, our own id and probe texts show.
QString who(const QString &id)
{
    if (!g_redact || Oscar::normalize(id) == Oscar::normalize(g_self) || id.length() <= 4)
        return id;
    return id.left(2) + QString(id.length() - 4, QLatin1Char('*')) + id.right(2);
}

// --trace: liboscar's debug output. With --redact only lines that describe
// packet structure (sizes, channels, families, TLV type names, task steps)
// are shown, and anything after a TLV name is cut, so no cookies, hashes or
// texts reach a public log.
bool g_trace = false;
QtMessageHandler g_defaultHandler = nullptr;
void out(const QString &line);

void traceHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (!ctx.category || qstrcmp(ctx.category, "liboscar") != 0) {
        if (g_defaultHandler) g_defaultHandler(type, ctx, msg);
        return;
    }
    QString line = msg.simplified();
    if (line.isEmpty())
        return;
    if (g_redact) {
        static const QRegularExpression safe(QStringLiteral(
            "^(Received \\d+ bytes|family: \\d+ subtype: \\d+|channel: \\d+ sequence: \\d+ length: \\d+|"
            "packet not long enough|packet size is \\d+|message was incomplete|Not enough bytes|"
            "SEND |RECV |Starting login|Sending the FLAP version|Sending the login cookie|Got the authorization key|"
            "stage one|stage two|no errors from stage|errors reported|Sending channel 0x04|Clearing all connections|"
            "Socket open|unknown wire format|start byte is|We should reconnect|Connect to: host|found TLV\\(\\d+\\)|"
            "error: |after error|emitting finished|Sending client ready|Sending SSI|Requesting|Got |Setting status)"));
        if (!safe.match(line).hasMatch())
            return;
        static const QRegularExpression tlv(QStringLiteral("^(found TLV\\(\\d+\\)( \\[[A-Z ]+\\])?).*$"));
        line.replace(tlv, QStringLiteral("\\1"));
        static const QRegularExpression host(QStringLiteral("^Connect to: host .*?(port \\d+ encrypted \\w+).*$"));
        line.replace(host, QStringLiteral("Connect to: host <server> \\1"));
        static const QRegularExpression reconnect(QStringLiteral("^We should reconnect.*$"));
        line.replace(reconnect, QStringLiteral("We should reconnect to the BOS server"));
    }
    out(QStringLiteral("TRACE ") + line.left(200));
}

QString shown(const QString &text, const QString &allowed = QString())
{
    if (!g_redact || (!allowed.isEmpty() && text.contains(allowed)))
        return text;
    return QStringLiteral("<%1 characters>").arg(text.length());
}
int g_received = 0;
int g_acked = 0;

void out(const QString &line)
{
    const QString ts = QDateTime::currentDateTimeUtc().toString(QStringLiteral("hh:mm:ss.zzz"));
    fprintf(stdout, "%s %s\n", qPrintable(ts), qPrintable(line));
    fflush(stdout);
}

class Utf8Codecs : public Client::CodecProvider
{
public:
    QTextCodec *codecForContact(const QString &) const override { return QTextCodec::codecForName("UTF-8"); }
    QTextCodec *codecForAccount() const override { return QTextCodec::codecForName("UTF-8"); }
};

Oscar::ClientVersion identity(const QString &name)
{
    Oscar::ClientVersion v;
    v.country = QStringLiteral("us");
    v.lang = QStringLiteral("en");
    if (name == QLatin1String("icq2003b")) {
        v.clientString = QStringLiteral("ICQ Inc. - Product of ICQ (TM).2003b.5.56.1.3916.85");
        v.clientId = 0x010A; v.major = 0x0005; v.minor = 0x0038; v.point = 0x0001; v.build = 0x0F4C;
        v.other = 0x00000055;
    } else if (name == QLatin1String("aim51")) {
        v.clientString = QStringLiteral("AOL Instant Messenger (SM), version 5.1.3036/WIN32");
        v.clientId = 0x0109; v.major = 0x0005; v.minor = 0x0001; v.point = 0x0000; v.build = 0x0BDC;
        v.other = 0x000000D2;
    } else { // icq65 (Kopete's default ICQ identity)
        v.clientString = QStringLiteral("ICQ Client");
        v.clientId = 0x010A; v.major = 0x0006; v.minor = 0x0005; v.point = 0x0000; v.build = 0x0412;
        v.other = 0x00000000;
    }
    return v;
}

// Sign-on error codes (TLV 0x08 of SNAC 17/03, or the channel-4 sign-off).
QString loginErrorName(int code)
{
    switch (code) {
    case 0x01: case 0x04: case 0x05: return QStringLiteral("wrong number/screen name or password");
    case 0x02: return QStringLiteral("service temporarily unavailable");
    case 0x07: case 0x08: return QStringLiteral("account does not exist");
    case 0x0C: return QStringLiteral("internal server error");
    case 0x11: return QStringLiteral("account suspended");
    case 0x14: return QStringLiteral("service temporarily offline");
    case 0x18: return QStringLiteral("rate limit exceeded");
    case 0x1B: case 0x1C: return QStringLiteral("client version too old");
    case 0x1D: return QStringLiteral("reconnecting too fast (rate limited); wait before retrying");
    case 0x20: return QStringLiteral("invalid SecurID");
    default: return QStringLiteral("unknown");
    }
}

Oscar::DWORD statusFromName(const QString &name)
{
    if (name == QLatin1String("away")) return Oscar::STATUS_AWAY;
    if (name == QLatin1String("na")) return Oscar::STATUS_NA | Oscar::STATUS_AWAY;
    if (name == QLatin1String("occupied")) return Oscar::STATUS_OCCUPIED | Oscar::STATUS_AWAY;
    if (name == QLatin1String("dnd")) return Oscar::STATUS_DND | Oscar::STATUS_OCCUPIED | Oscar::STATUS_AWAY;
    if (name == QLatin1String("ffc")) return Oscar::STATUS_FREE4CHAT;
    if (name == QLatin1String("invisible")) return Oscar::STATUS_INVISIBLE;
    return Oscar::STATUS_ONLINE;
}

} // namespace

int main(int argc, char **argv)
{
    // liboscar uses QTextDocument for rich text, which needs a QGuiApplication.
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("minimal") : qgetenv("QT_QPA_PLATFORM"));
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("oscarprobe"));

    QCommandLineParser p;
    p.setApplicationDescription(QStringLiteral("Sign on to an OSCAR server and report what happens."));
    p.addHelpOption();
    p.addOptions({
        {QStringLiteral("host"), QStringLiteral("Login server."), QStringLiteral("host"), QStringLiteral("login.oscar.nina.chat")},
        {QStringLiteral("port"), QStringLiteral("Login port."), QStringLiteral("port"), QStringLiteral("5190")},
        {QStringLiteral("tls"), QStringLiteral("Use SSL/TLS to the login server.")},
        {QStringLiteral("uin"), QStringLiteral("ICQ number or AIM screen name."), QStringLiteral("id")},
        {QStringLiteral("password-env"), QStringLiteral("Environment variable holding the password."), QStringLiteral("var"), QStringLiteral("OSCAR_PASSWORD")},
        {QStringLiteral("identity"), QStringLiteral("Client identity: icq65, icq2003b or aim51."), QStringLiteral("name"), QStringLiteral("icq65")},
        {QStringLiteral("status"), QStringLiteral("online, away, na, occupied, dnd, ffc or invisible."), QStringLiteral("status"), QStringLiteral("online")},
        {QStringLiteral("to"), QStringLiteral("Send a message to this ICQ number or screen name."), QStringLiteral("id")},
        {QStringLiteral("message"), QStringLiteral("Text to send with --to."), QStringLiteral("text"), QStringLiteral("Hello from oscarprobe")},
        {QStringLiteral("send-delay"), QStringLiteral("Seconds after the contact list arrives before sending."), QStringLiteral("s"), QStringLiteral("2")},
        {QStringLiteral("expect-message"), QStringLiteral("Exit with an error unless a message with this text arrives."), QStringLiteral("text")},
        {QStringLiteral("wait"), QStringLiteral("Seconds to stay signed on."), QStringLiteral("s"), QStringLiteral("15")},
        {QStringLiteral("redact"), QStringLiteral("Mask other users' ids and message texts in the output.")},
        {QStringLiteral("trace"), QStringLiteral("Show liboscar's protocol trace (structure only with --redact).")},
    });
    p.process(app);

    const QString uin = p.value(QStringLiteral("uin"));
    const QByteArray passwordVar = p.value(QStringLiteral("password-env")).toLatin1();
    const QString password = QString::fromUtf8(qgetenv(passwordVar.constData()));
    if (uin.isEmpty() || password.isEmpty()) {
        fprintf(stderr, "oscarprobe: --uin and a password in $%s are required\n", passwordVar.constData());
        return 2;
    }
    const QString host = p.value(QStringLiteral("host"));
    const quint16 port = p.value(QStringLiteral("port")).toUShort();
    const bool tls = p.isSet(QStringLiteral("tls"));
    const bool isIcq = !uin.isEmpty() && uin.at(0).isDigit();
    const QString to = p.value(QStringLiteral("to"));
    const QString expect = p.value(QStringLiteral("expect-message"));
    g_redact = p.isSet(QStringLiteral("redact"));
    g_self = uin;
    g_trace = p.isSet(QStringLiteral("trace"));
    if (g_trace) {
        QLoggingCategory::setFilterRules(QStringLiteral("liboscar.debug=true"));
        g_defaultHandler = qInstallMessageHandler(traceHandler);
    }

    Utf8Codecs codecs;
    static Oscar::ClientVersion version;
    version = identity(p.value(QStringLiteral("identity")));

    Client client;
    client.setIsIcq(isIcq);
    client.setVersion(&version);
    client.setCodecProvider(&codecs);

    out(QStringLiteral("CONFIG host=%1 port=%2 tls=%3 account=%4 icq=%5 identity=\"%6\" password_length=%7")
        .arg(host).arg(port).arg(tls ? "yes" : "no").arg(uin).arg(isIcq ? "yes" : "no")
        .arg(version.clientString).arg(password.length()));

    QObject::connect(&client, &Client::loggedIn, [&]() {
        g_loggedIn = true;
        out(QStringLiteral("LOGIN ok"));
    });
    QObject::connect(&client, &Client::loginFailed, [&]() {
        out(QStringLiteral("LOGIN failed"));
        g_exitCode = 1;
        QTimer::singleShot(0, &app, &QCoreApplication::quit);
    });
    QObject::connect(&client, &Client::socketError, [&](int code, const QString &msg) {
        out(QStringLiteral("SOCKET-ERROR %1 %2").arg(code).arg(msg));
    });
    QObject::connect(&client, &Client::taskError, [&](const Oscar::SNAC &s, int code, bool fatal) {
        const bool signOn = s.family == 0 && s.subtype == 0;
        out(QStringLiteral("TASK-ERROR family=0x%1 subtype=0x%2 code=%3 fatal=%4%5")
            .arg(s.family, 4, 16, QLatin1Char('0')).arg(s.subtype, 4, 16, QLatin1Char('0'))
            .arg(code).arg(fatal ? "yes" : "no")
            .arg(signOn ? QStringLiteral(" meaning=\"%1\"").arg(loginErrorName(code)) : QString()));
        if (fatal) {
            g_exitCode = 1;
            QTimer::singleShot(200, &app, &QCoreApplication::quit);
        }
    });
    QObject::connect(&client, &Client::disconnected, [&]() {
        out(QStringLiteral("DISCONNECTED"));
    });
    QObject::connect(&client, &Client::connectedElsewhere, [&]() {
        out(QStringLiteral("CONNECTED-ELSEWHERE"));
    });
    QObject::connect(&client, &Client::haveOwnInfo, [&]() {
        const UserDetails me = client.ourInfo();
        out(QStringLiteral("OWN-INFO id=%1 class=0x%2 status=0x%3 online_since=%4")
            .arg(me.userId()).arg(me.userClass(), 0, 16).arg(me.extendedStatus(), 0, 16)
            .arg(me.onlineSinceTime().toString(Qt::ISODate)));
    });
    QObject::connect(&client, &Client::haveContactList, [&]() {
        ContactManager *ssi = client.ssiManager();
        const QList<OContact> groups = ssi->groupList();
        const QList<OContact> contacts = ssi->contactList();
        out(QStringLiteral("CONTACT-LIST groups=%1 contacts=%2").arg(groups.size()).arg(contacts.size()));
        for (const OContact &g : groups)
            out(QStringLiteral("  GROUP \"%1\" gid=%2").arg(shown(g.name())).arg(g.gid()));
        for (const OContact &c : contacts)
            out(QStringLiteral("  CONTACT %1 alias=\"%2\" gid=%3 awaiting_auth=%4")
                .arg(who(c.name()), shown(c.alias())).arg(c.gid()).arg(c.waitingAuth() ? "yes" : "no"));
        if (!to.isEmpty()) {
            const int delayMs = p.value(QStringLiteral("send-delay")).toInt() * 1000;
            QTimer::singleShot(delayMs, [&]() {
                Oscar::Message msg;
                const QString text = p.value(QStringLiteral("message"));
                msg.setText(Oscar::Message::encodingForText(text, true), text, codecs.codecForContact(to));
                msg.setId(1);
                msg.setReceiver(to);
                msg.setSender(uin);
                msg.setTimestamp(QDateTime::currentDateTime());
                msg.setChannel(0x01);
                client.sendMessage(msg);
                out(QStringLiteral("SENT to=%1 text=\"%2\"").arg(who(to), text));
            });
        }
    });
    QObject::connect(&client, &Client::userIsOnline, [&](const QString &who) {
        out(QStringLiteral("ONLINE %1").arg(::who(who)));
    });
    QObject::connect(&client, &Client::userIsOffline, [&](const QString &who) {
        out(QStringLiteral("OFFLINE %1").arg(::who(who)));
    });
    QObject::connect(&client, &Client::messageReceived, [&](const Oscar::Message &m) {
        ++g_received;
        const QString text = m.text(codecs.codecForContact(m.sender()));
        out(QStringLiteral("MESSAGE from=%1 channel=%2 properties=0x%3 sent=%4 text=\"%5\"")
            .arg(who(m.sender())).arg(m.channel()).arg(m.properties(), 0, 16)
            .arg(m.timestamp().toUTC().toString(Qt::ISODate)).arg(shown(text, expect.isEmpty() ? QStringLiteral("oscarprobe") : expect)));
        if (!expect.isEmpty() && text.contains(expect)) {
            g_expectedSeen = true;
            out(QStringLiteral("EXPECTED-MESSAGE received"));
            QTimer::singleShot(500, &app, &QCoreApplication::quit);
        }
    });
    QObject::connect(&client, &Client::messageAck, [&](const QString &who, uint id) {
        ++g_acked;
        out(QStringLiteral("ACK from=%1 id=%2").arg(::who(who)).arg(id));
    });
    QObject::connect(&client, &Client::messageError, [&](const QString &who, uint id) {
        out(QStringLiteral("MESSAGE-ERROR to=%1 id=%2").arg(::who(who)).arg(id));
    });
    QObject::connect(&client, &Client::authRequestReceived, [&](const QString &who, const QString &reason) {
        out(QStringLiteral("AUTH-REQUEST from=%1 reason=\"%2\"").arg(::who(who), shown(reason)));
    });
    QObject::connect(&client, &Client::userStartedTyping, [&](const QString &who) {
        out(QStringLiteral("TYPING %1").arg(::who(who)));
    });

    client.start(host, port, uin, password);
    client.setStatus(statusFromName(p.value(QStringLiteral("status"))));
    client.connectToServer(host, port, tls, tls ? host : QString());

    const int waitMs = p.value(QStringLiteral("wait")).toInt() * 1000;
    QTimer::singleShot(waitMs, &app, &QCoreApplication::quit);
    app.exec();

    if (g_loggedIn) {
        client.close();
        QCoreApplication::processEvents();
    }
    if (!g_loggedIn && g_exitCode == 0)
        g_exitCode = 1;
    if (!expect.isEmpty() && g_exitCode == 0 && !g_expectedSeen)
        g_exitCode = 3;
    out(QStringLiteral("RESULT login=%1 received=%2 acked=%3 exit=%4")
        .arg(g_loggedIn ? "ok" : "failed").arg(g_received).arg(g_acked).arg(g_exitCode));
    return g_exitCode;
}
