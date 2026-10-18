// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "accountmanager.h"

#include <QDir>
#include <QNetworkConfigurationManager>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSettings>
#include <QTextCodec>
#include <QTextDocument>

#include "chatmodel.h"

#include "client.h"
#include "contact.h"
#include "contactmanager.h"
#include "oscarmessage.h"
#include "oscarsettings.h"
#include "oscartypes.h"
#include "oscarutils.h"
#include "rtf2html.h"
#include "userdetails.h"

namespace {

// Every account talks UTF-8 to liboscar; liboscar picks the wire encoding.
class Utf8CodecProvider : public Client::CodecProvider
{
public:
    QTextCodec *codecForContact(const QString &) const override { return QTextCodec::codecForName("UTF-8"); }
    QTextCodec *codecForAccount() const override { return QTextCodec::codecForName("UTF-8"); }
};
Utf8CodecProvider g_codecs;

const int kConnectTimeoutMs = 60 * 1000;
const int kLivenessTimeoutMs = 45 * 1000;
const int kBackoffSeconds[] = { 5, 15, 30, 60, 120, 300 };
const int kRateLimitedSeconds = 5 * 60;

Oscar::DWORD wireStatus(int status)
{
    switch (status) {
    case AccountManager::StatusAway: return Oscar::STATUS_AWAY;
    case AccountManager::StatusNotAvailable: return Oscar::STATUS_NA | Oscar::STATUS_AWAY;
    case AccountManager::StatusOccupied: return Oscar::STATUS_OCCUPIED | Oscar::STATUS_AWAY;
    case AccountManager::StatusDoNotDisturb: return Oscar::STATUS_DND | Oscar::STATUS_OCCUPIED | Oscar::STATUS_AWAY;
    case AccountManager::StatusFreeForChat: return Oscar::STATUS_FREE4CHAT;
    case AccountManager::StatusInvisible: return Oscar::STATUS_INVISIBLE;
    default: return Oscar::STATUS_ONLINE;
    }
}

int presenceFromDetails(const UserDetails &d)
{
    if (d.extendedStatusSpecified()) {
        const Oscar::DWORD st = d.extendedStatus() & 0xFFFF;
        if (st & Oscar::STATUS_DND) return ContactModel::DoNotDisturb;
        if (st & Oscar::STATUS_OCCUPIED) return ContactModel::Occupied;
        if (st & Oscar::STATUS_NA) return ContactModel::NotAvailable;
        if (st & Oscar::STATUS_AWAY) return ContactModel::Away;
        if (st & Oscar::STATUS_FREE4CHAT) return ContactModel::FreeForChat;
        if (st & Oscar::STATUS_INVISIBLE) return ContactModel::Invisible;
        return ContactModel::Online;
    }
    return (d.userClass() & Oscar::CLASS_AWAY) ? ContactModel::Away : ContactModel::Online;
}

// Incoming text may be plain, AIM-style HTML or ICQ RTF; show it as plain text.
QString toPlainText(const QString &in)
{
    QString text = in;
    if (text.startsWith(QLatin1String("{\\rtf"))) {
        RTF2HTML parser;
        text = parser.Parse(text.toLatin1().constData(), "utf-8");
    }
    static const QRegularExpression markup(QStringLiteral("<\\s*/?\\s*(html|body|font|b|i|u|br|span|p|div|a)\\b"),
                                           QRegularExpression::CaseInsensitiveOption);
    if (text.contains(markup)) {
        QTextDocument doc;
        doc.setHtml(text);
        text = doc.toPlainText();
    }
    return text.trimmed();
}

Oscar::ClientVersion *makeVersion(bool icq)
{
    Oscar::ClientVersion *v = new Oscar::ClientVersion;
    v->country = QStringLiteral("us");
    v->lang = QStringLiteral("en");
    if (icq) { // ICQ 6.5, the version NINA recommends
        v->clientString = QStringLiteral("ICQ Client");
        v->clientId = 0x010A; v->major = 0x0006; v->minor = 0x0005; v->point = 0x0000; v->build = 0x0412;
        v->other = 0x00000000;
    } else {   // AIM 5.1
        v->clientString = QStringLiteral("AOL Instant Messenger (SM), version 5.1.3036/WIN32");
        v->clientId = 0x0109; v->major = 0x0005; v->minor = 0x0001; v->point = 0x0000; v->build = 0x0BDC;
        v->other = 0x000000D2;
    }
    return v;
}

} // namespace

AccountManager::AccountManager(const QString &configDir, const QString &dataDir, QObject *parent)
    : QObject(parent),
      m_configDir(configDir),
      m_dataDir(dataDir),
      m_contacts(new ContactModel(this)),
      m_network(new QNetworkConfigurationManager(this))
{
    QDir().mkpath(m_configDir);
    QDir().mkpath(m_dataDir);

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &AccountManager::reconnect);
    m_livenessTimer.setSingleShot(true);
    connect(&m_livenessTimer, &QTimer::timeout, this, &AccountManager::onLivenessTimeout);
    connect(m_contacts, &ContactModel::unreadChanged, this, &AccountManager::unreadTotalChanged);
    connect(m_network, &QNetworkConfigurationManager::onlineStateChanged, this, [this](bool online) {
        if (online && m_wantOnline && m_state == Offline) {
            m_retries = 0;
            reconnect();
        }
    });

    loadSettings();
    openHistory();
    QSettings s(m_configDir + QStringLiteral("/account.conf"), QSettings::IniFormat);
    if (hasAccount() && hasPassword() && s.value(QStringLiteral("wantOnline"), false).toBool())
        QTimer::singleShot(0, this, [this]() { connectAccount(); });
}

AccountManager::~AccountManager()
{
    dropClient();
    m_history.close();
    delete m_version;
}

bool AccountManager::isIcq() const
{
    return !m_uin.isEmpty() && m_uin.at(0).isDigit();
}

int AccountManager::reconnectSeconds() const
{
    if (!m_reconnectTimer.isActive())
        return 0;
    return qMax(0, int(QDateTime::currentDateTime().secsTo(m_reconnectAt)));
}

int AccountManager::unreadTotal() const
{
    return m_contacts->unreadTotal();
}

void AccountManager::loadSettings()
{
    QSettings s(m_configDir + QStringLiteral("/account.conf"), QSettings::IniFormat);
    m_uin = s.value(QStringLiteral("uin")).toString();
    m_password = s.value(QStringLiteral("password")).toString();
    m_rememberPassword = s.value(QStringLiteral("rememberPassword"), true).toBool();
    m_profile = s.value(QStringLiteral("profile"), QStringLiteral("nina-tls")).toString();
    m_host = s.value(QStringLiteral("host")).toString();
    m_port = s.value(QStringLiteral("port"), 5190).toInt();
    m_tls = s.value(QStringLiteral("tls"), true).toBool();
    m_status = s.value(QStringLiteral("status"), int(StatusOnline)).toInt();
    if (m_status == StatusOffline)
        m_status = StatusOnline;
    m_statusMessage = s.value(QStringLiteral("statusMessage")).toString();
    m_stayAwake = s.value(QStringLiteral("stayAwake"), false).toBool();
}

void AccountManager::saveSettings()
{
    QSettings s(m_configDir + QStringLiteral("/account.conf"), QSettings::IniFormat);
    if (m_uin.isEmpty()) {
        s.clear();
        s.setValue(QStringLiteral("stayAwake"), m_stayAwake);
        return;
    }
    s.setValue(QStringLiteral("stayAwake"), m_stayAwake);
    s.setValue(QStringLiteral("uin"), m_uin);
    if (m_rememberPassword && !m_password.isEmpty())
        s.setValue(QStringLiteral("password"), m_password);
    else
        s.remove(QStringLiteral("password"));
    s.setValue(QStringLiteral("rememberPassword"), m_rememberPassword);
    s.setValue(QStringLiteral("profile"), m_profile);
    s.setValue(QStringLiteral("host"), m_host);
    s.setValue(QStringLiteral("port"), m_port);
    s.setValue(QStringLiteral("tls"), m_tls);
    s.setValue(QStringLiteral("status"), m_status);
    s.setValue(QStringLiteral("statusMessage"), m_statusMessage);
    s.setValue(QStringLiteral("wantOnline"), m_wantOnline);
}

void AccountManager::openHistory()
{
    m_history.close();
    for (const QPointer<ChatModel> &c : m_chats)
        if (c) c->deleteLater();
    m_chats.clear();
    if (m_uin.isEmpty()) {
        m_contacts->setContacts({});
        return;
    }
    m_history.open(m_dataDir + QStringLiteral("/history.sqlite"), Oscar::normalize(m_uin));

    QVector<ContactModel::Item> items;
    for (const StoredContact &sc : m_history.contacts()) {
        ContactModel::Item it;
        it.uin = sc.uin;
        it.alias = sc.alias;
        it.group = sc.group;
        items.append(it);
    }
    for (const StoredMessage &last : m_history.lastMessages()) {
        bool found = false;
        for (ContactModel::Item &it : items) {
            if (Oscar::normalize(it.uin) == Oscar::normalize(last.contact)) {
                it.lastMessage = last.text;
                it.lastMessageTime = last.timestamp;
                it.unread = m_history.unreadCount(it.uin);
                found = true;
                break;
            }
        }
        if (!found) {
            ContactModel::Item it;
            it.uin = last.contact;
            it.lastMessage = last.text;
            it.lastMessageTime = last.timestamp;
            it.unread = m_history.unreadCount(last.contact);
            items.append(it);
        }
    }
    m_contacts->setContacts(items);
}

void AccountManager::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    if (s != Online)
        m_contacts->setAllOffline();
    emit stateChanged();
}

void AccountManager::setError(const QString &error)
{
    if (m_lastError == error)
        return;
    m_lastError = error;
    emit lastErrorChanged();
}

void AccountManager::signIn(const QString &uin, const QString &password, bool rememberPassword,
                            const QString &profile, const QString &host, int port, bool tls)
{
    QString id = uin.trimmed();
    if (!id.isEmpty() && id.at(0).isDigit())
        id.remove(QRegularExpression(QStringLiteral("[^0-9]")));
    if (id.isEmpty() || password.isEmpty()) {
        setError(tr("Enter your ICQ number (or screen name) and password."));
        return;
    }
    disconnectAccount();
    const bool sameAccount = Oscar::normalize(id) == Oscar::normalize(m_uin);
    m_uin = id;
    m_password = password;
    m_rememberPassword = rememberPassword;
    m_profile = profile;
    if (profile == QLatin1String("custom")) {
        m_host = host.trimmed();
        m_port = port > 0 ? port : 5190;
        m_tls = tls;
    }
    if (!sameAccount)
        openHistory();
    emit accountChanged();
    connectAccount();
}

void AccountManager::connectAccount(const QString &password)
{
    if (!password.isEmpty())
        m_password = password;
    if (!hasAccount()) {
        setError(tr("No account set up."));
        return;
    }
    if (!hasPassword()) {
        setError(tr("Enter your password to sign on."));
        return;
    }
    m_wantOnline = true;
    m_retries = 0;
    saveSettings();
    emit accountChanged();
    startClient();
}

void AccountManager::disconnectAccount()
{
    m_wantOnline = false;
    m_reconnectTimer.stop();
    m_livenessTimer.stop();
    dropClient();
    setState(Offline);
    saveSettings();
    emit stateChanged();
}

void AccountManager::forgetAccount()
{
    disconnectAccount();
    m_uin.clear();
    m_password.clear();
    m_statusMessage.clear();
    saveSettings();
    openHistory();
    setError(QString());
    emit accountChanged();
    emit statusChanged();
}

void AccountManager::setStatus(int status, const QString &message)
{
    if (status == StatusOffline) {
        disconnectAccount();
        return;
    }
    m_status = status;
    m_statusMessage = message;
    saveSettings();
    emit statusChanged();
    if (m_state == Online && m_client)
        m_client->setStatus(wireStatus(m_status), m_statusMessage);
    else if (m_state == Offline && hasAccount())
        connectAccount();
}

void AccountManager::startClient()
{
    dropClient();
    m_reconnectTimer.stop();
    setError(QString());
    setState(Connecting);

    const bool icq = isIcq();
    delete m_version;
    m_version = makeVersion(icq);

    QString host = m_host;
    int port = m_port;
    bool tls = m_tls;
    if (m_profile == QLatin1String("nina")) {
        host = QStringLiteral("login.oscar.nina.chat"); port = 5190; tls = false;
    } else if (m_profile != QLatin1String("custom")) { // nina-tls
        host = QStringLiteral("slogin.oscar.nina.chat"); port = 5190; tls = true;
    }

    m_client = new Client(this);
    m_client->setIsIcq(icq);
    m_client->setVersion(m_version);
    m_client->setCodecProvider(&g_codecs);
    // Join other sessions instead of knocking them off (NINA allows several).
    m_client->clientSettings()->setMultiConnection(0x01);

    connect(m_client, &Client::loggedIn, this, &AccountManager::onLoggedIn);
    connect(m_client, &Client::haveContactList, this, &AccountManager::onHaveContactList);
    connect(m_client, &Client::haveOwnInfo, this, &AccountManager::onHaveOwnInfo);
    connect(m_client, &Client::taskError, this, &AccountManager::onTaskError);
    connect(m_client, &Client::socketError, this, &AccountManager::onSocketError);
    connect(m_client, &Client::disconnected, this, &AccountManager::onDisconnected);
    connect(m_client, &Client::connectedElsewhere, this, &AccountManager::onConnectedElsewhere);
    connect(m_client, &Client::messageReceived, this, &AccountManager::onMessage);
    connect(m_client, &Client::messageAck, this, &AccountManager::onMessageAck);
    connect(m_client, &Client::messageError, this, &AccountManager::onMessageError);
    connect(m_client, &Client::receivedUserInfo, this, &AccountManager::onUserInfo);
    connect(m_client, &Client::userIsOffline, this, &AccountManager::onUserOffline);
    connect(m_client, &Client::userStartedTyping, this, &AccountManager::onTypingStarted);
    connect(m_client, &Client::userStoppedTyping, this, &AccountManager::onTypingStopped);
    connect(m_client, &Client::authRequestReceived, this, &AccountManager::onAuthRequest);
    connect(m_client, &Client::authReplyReceived, this, &AccountManager::onAuthReply);
    ContactManager *ssi = m_client->ssiManager();
    connect(ssi, &ContactManager::contactAdded, this, &AccountManager::onContactChanged);
    connect(ssi, &ContactManager::contactUpdated, this, &AccountManager::onContactChanged);
    connect(ssi, &ContactManager::contactRemoved, this, &AccountManager::onContactRemoved);

    m_client->start(host, port, m_uin, m_password);
    m_client->setStatus(wireStatus(m_status), m_statusMessage);
    m_client->connectToServer(host, port, tls, tls ? host : QString());
    m_livenessTimer.start(kConnectTimeoutMs);
}

void AccountManager::dropClient()
{
    if (!m_client)
        return;
    Oscar::Client *c = m_client;
    m_client = nullptr;
    disconnect(c, nullptr, this, nullptr);
    disconnect(c->ssiManager(), nullptr, this, nullptr);
    c->close();
    c->deleteLater();
    // Messages still waiting for an acknowledgement stay "sending".
    m_pendingAcks.clear();
}

void AccountManager::onLoggedIn()
{
    m_livenessTimer.stop();
    m_retries = 0;
    setError(QString());
    setState(Online);
}

QString AccountManager::groupName(int gid) const
{
    if (!m_client)
        return QString();
    for (const OContact &g : m_client->ssiManager()->groupList())
        if (g.gid() == gid)
            return g.name();
    return QString();
}

void AccountManager::rebuildContactsFromServer()
{
    if (!m_client)
        return;
    QVector<ContactModel::Item> items;
    QVector<StoredContact> cache;
    for (const OContact &c : m_client->ssiManager()->contactList()) {
        if (c.type() != Oscar::ROSTER_CONTACT || c.name().isEmpty())
            continue;
        ContactModel::Item it;
        it.uin = c.name();
        it.alias = c.alias();
        it.group = groupName(c.gid());
        it.awaitingAuth = c.waitingAuth();
        it.unread = m_history.unreadCount(it.uin);
        items.append(it);
        cache.append({ it.uin, it.alias, it.group });
    }
    for (const StoredMessage &last : m_history.lastMessages()) {
        for (ContactModel::Item &it : items) {
            if (Oscar::normalize(it.uin) == Oscar::normalize(last.contact)) {
                it.lastMessage = last.text;
                it.lastMessageTime = last.timestamp;
            }
        }
    }
    m_contacts->setContacts(items);
    m_history.saveContacts(cache);
}

void AccountManager::onHaveContactList()
{
    rebuildContactsFromServer();
}

void AccountManager::onContactChanged(const OContact &c)
{
    if (c.type() != Oscar::ROSTER_CONTACT || c.name().isEmpty())
        return;
    ContactModel::Item it;
    it.uin = c.name();
    it.alias = c.alias();
    it.group = groupName(c.gid());
    it.awaitingAuth = c.waitingAuth();
    m_contacts->upsert(it);
}

void AccountManager::onContactRemoved(const QString &contact)
{
    m_contacts->remove(contact);
}

void AccountManager::onHaveOwnInfo()
{
    if (m_state == Online)
        m_livenessTimer.stop();
}

void AccountManager::onTaskError(const Oscar::SNAC &snac, int code, bool fatal)
{
    if (snac.family == 0 && snac.subtype == 0) {
        // Sign-on errors from the authorizer.
        switch (code) {
        case 0x01: case 0x04: case 0x05:
            m_wantOnline = false;
            dropClient();
            setState(Offline);
            setError(tr("Wrong ICQ number or password. If you set a separate legacy password on nina.chat, use that one."));
            saveSettings();
            return;
        case 0x07: case 0x08:
            m_wantOnline = false;
            dropClient();
            setState(Offline);
            setError(tr("This account does not exist on the server."));
            saveSettings();
            return;
        case 0x11:
            m_wantOnline = false;
            dropClient();
            setState(Offline);
            setError(tr("This account is suspended."));
            saveSettings();
            return;
        case 0x1B: case 0x1C:
            m_wantOnline = false;
            dropClient();
            setState(Offline);
            setError(tr("The server says this client version is too old."));
            saveSettings();
            return;
        case 0x18: case 0x1D:
            dropClient();
            setError(tr("Signed on too often. Waiting a few minutes before trying again."));
            scheduleReconnect(kRateLimitedSeconds);
            return;
        default:
            connectionLost(tr("Sign-on failed (error %1).").arg(code));
            return;
        }
    }
    if (fatal)
        connectionLost(tr("The server closed the session (error %1).").arg(code));
}

void AccountManager::onSocketError(int, const QString &message)
{
    connectionLost(message.isEmpty() ? tr("Connection lost.") : message);
}

void AccountManager::onDisconnected()
{
    connectionLost(tr("Disconnected."));
}

void AccountManager::onConnectedElsewhere()
{
    m_wantOnline = false;
    dropClient();
    setState(Offline);
    setError(tr("Signed on from another place."));
    saveSettings();
}

void AccountManager::connectionLost(const QString &why)
{
    if (!m_client && m_state == Offline)
        return;
    m_livenessTimer.stop();
    dropClient();
    if (!m_wantOnline) {
        setState(Offline);
        return;
    }
    setError(why);
    const int n = int(sizeof(kBackoffSeconds) / sizeof(kBackoffSeconds[0]));
    scheduleReconnect(kBackoffSeconds[qMin(m_retries, n - 1)]);
    ++m_retries;
}

void AccountManager::scheduleReconnect(int seconds)
{
    m_reconnectAt = QDateTime::currentDateTime().addSecs(seconds);
    m_reconnectTimer.start(seconds * 1000);
    m_state = Offline;
    m_contacts->setAllOffline();
    emit stateChanged(); // also announces reconnectSeconds
}

void AccountManager::reconnect()
{
    if (m_wantOnline && hasAccount() && hasPassword() && !m_client)
        startClient();
}

void AccountManager::onLivenessTimeout()
{
    if (m_state == Connecting)
        connectionLost(tr("Connection timed out."));
    else if (m_state == Online)
        connectionLost(tr("No reply from the server."));
}

void AccountManager::keepAlive()
{
    if (m_state == Online && m_client) {
        m_client->requestOwnInfo();
        if (!m_livenessTimer.isActive())
            m_livenessTimer.start(kLivenessTimeoutMs);
    } else if (m_wantOnline && !m_client && m_reconnectTimer.isActive()
               && QDateTime::currentDateTime() >= m_reconnectAt) {
        // The timer may not have fired while the device slept.
        m_reconnectTimer.stop();
        reconnect();
    }
}

ChatModel *AccountManager::chat(const QString &uin)
{
    const QString key = Oscar::normalize(uin);
    QPointer<ChatModel> &c = m_chats[key];
    if (!c) {
        c = new ChatModel(uin, &m_history, this);
        QQmlEngine::setObjectOwnership(c, QQmlEngine::CppOwnership);
    }
    return c;
}

bool AccountManager::sendMessage(const QString &uin, const QString &rawText)
{
    const QString text = rawText.trimmed();
    if (text.isEmpty() || m_state != Online || !m_client)
        return false;

    const bool toIcq = !uin.isEmpty() && uin.at(0).isDigit();
    // AIM clients read HTML; ICQ clients read plain text.
    const QString wire = toIcq ? text : text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));

    Oscar::Message msg;
    msg.setText(Oscar::Message::encodingForText(wire, true), wire, g_codecs.codecForContact(uin));
    const uint id = m_nextMessageId++;
    msg.setId(id);
    msg.setReceiver(uin);
    msg.setSender(m_uin);
    msg.setTimestamp(QDateTime::currentDateTime());
    msg.setChannel(0x01);
    m_client->sendMessage(msg);

    StoredMessage m;
    m.contact = uin;
    m.outgoing = true;
    m.text = text;
    m.timestamp = QDateTime::currentDateTime();
    m.delivered = false;
    m.read = true;
    m.id = m_history.addMessage(m);
    m_pendingAcks.insert(id, qMakePair(Oscar::normalize(uin), m.id));
    if (ChatModel *c = m_chats.value(Oscar::normalize(uin)))
        c->append(m);
    m_contacts->setLastMessage(uin, text, m.timestamp);
    return true;
}

void AccountManager::onMessageAck(const QString &, uint id)
{
    if (!m_pendingAcks.contains(id))
        return;
    const QPair<QString, qint64> p = m_pendingAcks.take(id);
    m_history.setDelivered(p.second);
    if (ChatModel *c = m_chats.value(p.first))
        c->setDelivered(p.second);
}

void AccountManager::onMessageError(const QString &contact, uint id)
{
    m_pendingAcks.remove(id);
    emit messageFailed(contact);
}

void AccountManager::onMessage(const Oscar::Message &msg)
{
    if (msg.hasProperty(Oscar::Message::StatusMessageRequest))
        return;
    QString text = toPlainText(msg.text(g_codecs.codecForContact(msg.sender())));
    if (text.isEmpty())
        return;
    if (msg.hasProperty(Oscar::Message::AutoResponse))
        text = tr("Automatic reply: %1").arg(text);
    const QDateTime when = msg.timestamp().isValid() ? msg.timestamp() : QDateTime::currentDateTime();
    storeIncoming(msg.sender(), text, when);
}

void AccountManager::storeIncoming(const QString &uin, const QString &text, const QDateTime &when)
{
    const bool seen = m_appActive && Oscar::normalize(m_activeChat) == Oscar::normalize(uin);
    StoredMessage m;
    m.contact = uin;
    m.outgoing = false;
    m.text = text;
    m.timestamp = when;
    m.delivered = true;
    m.read = seen;
    m.id = m_history.addMessage(m);
    if (ChatModel *c = m_chats.value(Oscar::normalize(uin)))
        c->append(m);
    m_contacts->setTyping(uin, false);
    m_contacts->setLastMessage(uin, text, when);
    if (!seen) {
        m_contacts->setUnread(uin, m_history.unreadCount(uin));
        emit messageReceived(uin, displayName(uin), text);
    }
}

void AccountManager::markRead(const QString &uin)
{
    m_history.markRead(uin);
    m_contacts->setUnread(uin, 0);
}

void AccountManager::setActiveChat(const QString &uin)
{
    if (m_activeChat == uin)
        return;
    m_activeChat = uin;
    if (!uin.isEmpty() && m_appActive)
        markRead(uin);
    emit activeChatChanged();
}

void AccountManager::setAppActive(bool active)
{
    if (m_appActive == active)
        return;
    m_appActive = active;
    if (active && !m_activeChat.isEmpty())
        markRead(m_activeChat);
    emit appActiveChanged();
}

void AccountManager::setStayAwake(bool stay)
{
    if (m_stayAwake == stay)
        return;
    m_stayAwake = stay;
    saveSettings();
    emit stayAwakeChanged();
}

void AccountManager::setTyping(const QString &uin, bool typing)
{
    if (m_state == Online && m_client)
        m_client->sendTyping(uin, typing);
}

void AccountManager::onUserInfo(const QString &contact, const UserDetails &details)
{
    if (Oscar::normalize(contact) == Oscar::normalize(m_uin))
        return;
    m_contacts->setPresence(contact, presenceFromDetails(details));
}

void AccountManager::onUserOffline(const QString &contact)
{
    m_contacts->setPresence(contact, ContactModel::Offline);
}

void AccountManager::onTypingStarted(const QString &contact)
{
    m_contacts->setTyping(contact, true);
}

void AccountManager::onTypingStopped(const QString &contact)
{
    m_contacts->setTyping(contact, false);
}

void AccountManager::onAuthRequest(const QString &contact, const QString &reason)
{
    emit authorizationRequested(contact, displayName(contact), reason);
}

void AccountManager::answerAuthorization(const QString &uin, bool grant)
{
    if (m_state == Online && m_client)
        m_client->sendAuth(uin, QString(), grant);
}

void AccountManager::onAuthReply(const QString &contact, const QString &, bool granted)
{
    emit authorizationAnswered(contact, granted);
}

QString AccountManager::displayName(const QString &uin) const
{
    return m_contacts->displayName(uin);
}

QString AccountManager::presenceName(int presence)
{
    switch (presence) {
    case StatusOnline: return tr("Online");
    case StatusAway: return tr("Away");
    case StatusNotAvailable: return tr("Not available");
    case StatusOccupied: return tr("Occupied");
    case StatusDoNotDisturb: return tr("Do not disturb");
    case StatusFreeForChat: return tr("Free for chat");
    case StatusInvisible: return tr("Invisible");
    default: return tr("Offline");
    }
}
