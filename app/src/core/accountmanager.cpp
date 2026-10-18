// Part of harbour_icq_nina; GPL-3.0-or-later.
#include "accountmanager.h"

#include <QDir>
#include <QFile>
#include <QUrl>
#include <QFileInfo>
#include <QNetworkConfigurationManager>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextCodec>
#include <QTextDocument>

#include "chatmodel.h"
#include "linkupload.h"

#include "client.h"
#include "contact.h"
#include "contactmanager.h"
#include "filetransferhandler.h"
#include "icquserinfo.h"
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

// NINA's rendezvous proxy for file transfers. NINA's patcher maps AOL's
// *.aol.com hosts to *.nina.bz (same length), so AOL's ars.oscar.aol.com
// becomes this one.
const char kNinaProxyHost[] = "ars.oscar.nina.bz";

// Text from old ICQ clients and the ICQ directory: UTF-8 when it is valid
// UTF-8, otherwise Windows-1252.
QString legacyText(const QByteArray &bytes)
{
    QTextCodec::ConverterState state;
    const QString text = QTextCodec::codecForName("UTF-8")->toUnicode(bytes.constData(), bytes.size(), &state);
    if (state.invalidChars == 0)
        return text;
    if (QTextCodec *cp = QTextCodec::codecForName("windows-1252"))
        return cp->toUnicode(bytes);
    return QString::fromLatin1(bytes);
}

QString countryName(int code)
{
    // ICQ stores countries as telephone codes.
    static const QHash<int, const char *> names = {
        { 1, "United States" }, { 7, "Russia" }, { 20, "Egypt" }, { 27, "South Africa" },
        { 30, "Greece" }, { 31, "Netherlands" }, { 32, "Belgium" }, { 33, "France" },
        { 34, "Spain" }, { 36, "Hungary" }, { 39, "Italy" }, { 40, "Romania" },
        { 41, "Switzerland" }, { 42, "Czech Republic" }, { 43, "Austria" }, { 44, "United Kingdom" },
        { 45, "Denmark" }, { 46, "Sweden" }, { 47, "Norway" }, { 48, "Poland" },
        { 49, "Germany" }, { 51, "Peru" }, { 52, "Mexico" }, { 54, "Argentina" },
        { 55, "Brazil" }, { 56, "Chile" }, { 57, "Colombia" }, { 58, "Venezuela" },
        { 60, "Malaysia" }, { 61, "Australia" }, { 62, "Indonesia" }, { 63, "Philippines" },
        { 64, "New Zealand" }, { 65, "Singapore" }, { 66, "Thailand" }, { 81, "Japan" },
        { 82, "South Korea" }, { 84, "Vietnam" }, { 86, "China" }, { 90, "Turkey" },
        { 91, "India" }, { 92, "Pakistan" }, { 98, "Iran" }, { 107, "Canada" },
        { 112, "Kazakhstan" }, { 350, "Gibraltar" }, { 351, "Portugal" }, { 352, "Luxembourg" },
        { 353, "Ireland" }, { 354, "Iceland" }, { 356, "Malta" }, { 357, "Cyprus" },
        { 358, "Finland" }, { 359, "Bulgaria" }, { 370, "Lithuania" }, { 371, "Latvia" },
        { 372, "Estonia" }, { 373, "Moldova" }, { 374, "Armenia" }, { 375, "Belarus" },
        { 380, "Ukraine" }, { 381, "Serbia" }, { 385, "Croatia" }, { 386, "Slovenia" },
        { 387, "Bosnia and Herzegovina" }, { 389, "North Macedonia" }, { 420, "Czech Republic" },
        { 421, "Slovakia" }, { 852, "Hong Kong" }, { 886, "Taiwan" }, { 972, "Israel" },
        { 994, "Azerbaijan" }, { 995, "Georgia" }, { 998, "Uzbekistan" },
    };
    if (code <= 0)
        return QString();
    const char *n = names.value(code);
    return n ? QString::fromLatin1(n) : QString::number(code);
}

} // namespace

AccountManager::AccountManager(const QString &configDir, const QString &dataDir, QObject *parent,
                               const QString &downloadDir)
    : QObject(parent),
      m_configDir(configDir),
      m_dataDir(dataDir),
      m_contacts(new ContactModel(this)),
      m_network(new QNetworkConfigurationManager(this)),
      m_search(new SearchModel(this))
{
    QDir().mkpath(m_configDir);
    QDir().mkpath(m_dataDir);
    m_transfers = new TransferManager(this, downloadDir.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) : downloadDir);
    m_searchTimer.setSingleShot(true);
    m_searchTimer.setInterval(20000);
    connect(&m_searchTimer, &QTimer::timeout, this, &AccountManager::finishSearch);

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
    m_proxyTransfers = s.value(QStringLiteral("proxyTransfers"), true).toBool();
    m_messageSound = s.value(QStringLiteral("messageSound"), 1).toInt();
    m_customSoundFile = s.value(QStringLiteral("customSoundFile")).toString();
    m_customSoundName = s.value(QStringLiteral("customSoundName")).toString();
    if (m_messageSound == 2 && !QFileInfo::exists(m_customSoundFile))
        m_messageSound = 1;
}

void AccountManager::saveSettings()
{
    QSettings s(m_configDir + QStringLiteral("/account.conf"), QSettings::IniFormat);
    if (m_uin.isEmpty()) {
        s.clear();
        s.setValue(QStringLiteral("stayAwake"), m_stayAwake);
        s.setValue(QStringLiteral("proxyTransfers"), m_proxyTransfers);
        s.setValue(QStringLiteral("messageSound"), m_messageSound);
        s.setValue(QStringLiteral("customSoundFile"), m_customSoundFile);
        s.setValue(QStringLiteral("customSoundName"), m_customSoundName);
        return;
    }
    s.setValue(QStringLiteral("messageSound"), m_messageSound);
    s.setValue(QStringLiteral("customSoundFile"), m_customSoundFile);
    s.setValue(QStringLiteral("customSoundName"), m_customSoundName);
    s.setValue(QStringLiteral("stayAwake"), m_stayAwake);
    s.setValue(QStringLiteral("proxyTransfers"), m_proxyTransfers);
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
    connect(m_client, &Client::contactEditFailed, this, &AccountManager::onContactEditFailed);
    connect(m_client, &Client::contactAddedYou, this, &AccountManager::onContactAddedYou);
    connect(m_client, &Client::receivedIcqShortInfo, this, &AccountManager::onShortInfo);
    connect(m_client, &Client::receivedIcqLongInfo, this, &AccountManager::onLongInfo);
    connect(m_client, &Client::gotSearchResults, this, &AccountManager::onSearchResult);
    connect(m_client, &Client::endOfSearch, this, &AccountManager::onSearchEnd);
    connect(m_client, &Client::incomingFileTransfer, this, &AccountManager::onIncomingFile);
    ContactManager *ssi = m_client->ssiManager();
    connect(ssi, &ContactManager::contactAdded, this, &AccountManager::onContactChanged);
    connect(ssi, &ContactManager::contactUpdated, this, &AccountManager::onContactChanged);
    connect(ssi, &ContactManager::contactRemoved, this, &AccountManager::onContactRemoved);
    connect(ssi, &ContactManager::groupAdded, this, &AccountManager::onGroupChanged);
    connect(ssi, &ContactManager::groupUpdated, this, &AccountManager::onGroupChanged);
    connect(ssi, &ContactManager::groupRemoved, this, &AccountManager::onGroupChanged);
    connect(ssi, &ContactManager::itemChanged, this, &AccountManager::refreshGroupsAndPrivacy);
    applyFileSettings();
    m_transfers->setClient(m_client);

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
    m_transfers->setClient(nullptr);
    if (m_searching)
        finishSearch();
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
    refreshGroupsAndPrivacy();
}

void AccountManager::onHaveContactList()
{
    rebuildContactsFromServer();
    flushPendingAdds();
    if (isIcq() && m_client)
        m_client->requestFullInfo(m_uin); // our nickname and "authorization required"
}

void AccountManager::onContactChanged(const OContact &c)
{
    if (c.type() != Oscar::ROSTER_CONTACT || c.name().isEmpty()) {
        refreshGroupsAndPrivacy(); // permit/deny/ignore items and settings
        return;
    }
    ContactModel::Item it;
    it.uin = c.name();
    it.alias = c.alias();
    it.group = groupName(c.gid());
    it.awaitingAuth = c.waitingAuth();
    m_contacts->upsert(it);
    m_history.saveContacts([this]() {
        QVector<StoredContact> cache;
        for (const ContactModel::Item &i : m_contacts->items())
            if (!i.group.isEmpty())
                cache.append({ i.uin, i.alias, i.group });
        return cache;
    }());

    // Added with "authorization required": now ask for it.
    const QString key = normalized(c.name());
    if (m_authReasons.contains(key)) {
        const QString reason = m_authReasons.take(key);
        if (c.waitingAuth() && m_client)
            m_client->requestAuth(c.name(), reason);
    }
}

void AccountManager::onContactRemoved(const QString &contact)
{
    // The ignore/visible/invisible lists use the same signal.
    refreshGroupsAndPrivacy();
    if (m_client && m_client->ssiManager()->findContact(Oscar::normalize(contact)))
        return;
    if (!m_contacts->contains(contact) || m_contacts->items().isEmpty())
        return;
    // Keep the conversation (with its history) in the list, outside any group.
    ContactModel::Item it;
    it.uin = contact;
    m_contacts->remove(contact);
    if (!m_history.messages(contact, 1).isEmpty()) {
        m_contacts->upsert(it);
        for (const StoredMessage &last : m_history.lastMessages())
            if (normalized(last.contact) == normalized(contact))
                m_contacts->setLastMessage(contact, last.text, last.timestamp);
    }
}

void AccountManager::onGroupChanged()
{
    refreshGroupsAndPrivacy();
    flushPendingAdds();
}

QString AccountManager::normalized(const QString &uin)
{
    return Oscar::normalize(uin);
}

void AccountManager::refreshGroupsAndPrivacy()
{
    if (!m_client)
        return;
    ContactManager *ssi = m_client->ssiManager();
    QStringList groups;
    for (const OContact &g : ssi->groupList())
        if (g.gid() != 0 && !g.name().isEmpty())
            groups << g.name();
    groups.sort(Qt::CaseInsensitive);
    if (groups != m_groups) {
        m_groups = groups;
        emit groupsChanged();
        // Contacts show their group's name; a rename changes it.
        for (const OContact &c : ssi->contactList()) {
            if (c.type() != Oscar::ROSTER_CONTACT || c.name().isEmpty())
                continue;
            ContactModel::Item it;
            it.uin = c.name();
            it.alias = c.alias();
            it.group = groupName(c.gid());
            it.awaitingAuth = c.waitingAuth();
            m_contacts->upsert(it);
        }
    }

    auto names = [](const QList<OContact> &items) {
        QStringList out;
        for (const OContact &c : items)
            if (!c.name().isEmpty())
                out << c.name();
        out.sort(Qt::CaseInsensitive);
        return out;
    };
    const QStringList blocked = names(ssi->ignoreList());
    const QStringList visible = names(ssi->visibleList());
    const QStringList invisible = names(ssi->invisibleList());
    int mode = PrivacyUnknown;
    const OContact settings = ssi->visibilityItem();
    if (settings) {
        const Oscar::TLV t = Oscar::findTLV(settings.tlvList(), 0x00CA);
        if (t && t.length >= 1)
            mode = quint8(t.data.at(0));
    }
    if (blocked != m_blocked || visible != m_visibleTo || invisible != m_invisibleTo || mode != m_privacyMode) {
        m_blocked = blocked;
        m_visibleTo = visible;
        m_invisibleTo = invisible;
        m_privacyMode = mode;
        emit privacyChanged();
    }
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
    return sendText(uin, rawText, true);
}

bool AccountManager::sendText(const QString &uin, const QString &rawText, bool record)
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
    if (!record)
        return true;

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
    if (isBlocked(msg.sender()))
        return;
    // Old ICQ "typed" messages (channel 4) separate their fields with 0xFE.
    const int type = msg.messageType();
    if (type == Oscar::MessageType::AuthRequest || type == Oscar::MessageType::AuthGranted
            || type == Oscar::MessageType::AuthDeny || type == Oscar::MessageType::Added) {
        const QList<QByteArray> fields = msg.textArray().split(char(0xFE));
        if (type == Oscar::MessageType::AuthRequest)
            onAuthRequest(msg.sender(), legacyText(fields.value(5, fields.value(0))));
        else if (type == Oscar::MessageType::AuthGranted)
            onAuthReply(msg.sender(), QString(), true);
        else if (type == Oscar::MessageType::AuthDeny)
            onAuthReply(msg.sender(), legacyText(fields.value(0)), false);
        else
            onContactAddedYou(msg.sender());
        return;
    }
    QString text;
    if (type == Oscar::MessageType::Url) {
        const QList<QByteArray> fields = msg.textArray().split(char(0xFE));
        text = (legacyText(fields.value(0)) + QLatin1Char(' ') + legacyText(fields.value(1))).trimmed();
    } else {
        text = toPlainText(msg.text(g_codecs.codecForContact(msg.sender())));
    }
    if (text.isEmpty())
        return;
    if (msg.hasProperty(Oscar::Message::AutoResponse))
        text = tr("Automatic reply: %1").arg(text);
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime when = msg.timestamp().isValid() ? msg.timestamp() : now;
    // A message stored while we were offline may be delivered again (another
    // session, or a reconnect before the server deleted it).
    if (when.secsTo(now) > 60 && m_history.hasIncoming(msg.sender(), text, when, 60))
        return;
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

void AccountManager::deleteConversation(const QString &uin)
{
    if (isContact(uin))
        return;
    m_history.deleteConversation(uin);
    if (ChatModel *c = m_chats.value(normalized(uin)))
        c->reload();
    m_contacts->remove(uin);
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
    if (isBlocked(contact))
        return;
    emit authorizationRequested(contact, displayName(contact), reason);
}

void AccountManager::answerAuthorization(const QString &uin, bool grant)
{
    if (m_state == Online && m_client)
        m_client->sendAuth(uin, QString(), grant);
}

void AccountManager::onAuthReply(const QString &contact, const QString &, bool granted)
{
    // The server clears the "awaiting authorization" flag in the contact
    // list; reflect it straight away.
    if (granted && m_client) {
        const OContact c = m_client->ssiManager()->findContact(Oscar::normalize(contact));
        if (c) {
            ContactModel::Item it;
            it.uin = c.name();
            it.alias = c.alias();
            it.group = groupName(c.gid());
            it.awaitingAuth = false;
            m_contacts->upsert(it);
        }
    }
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

// ---- Phase 2: contact list management -------------------------------------

bool AccountManager::isContact(const QString &uin) const
{
    if (!m_client)
        return false;
    const OContact c = m_client->ssiManager()->findContact(Oscar::normalize(uin));
    return c && c.type() == Oscar::ROSTER_CONTACT;
}

QString AccountManager::groupOf(const QString &uin) const
{
    if (!m_client)
        return QString();
    const OContact c = m_client->ssiManager()->findContact(Oscar::normalize(uin));
    return c ? groupName(c.gid()) : QString();
}

QString AccountManager::aliasOf(const QString &uin) const
{
    if (!m_client)
        return QString();
    const OContact c = m_client->ssiManager()->findContact(Oscar::normalize(uin));
    return c ? c.alias() : QString();
}

int AccountManager::groupSize(const QString &name) const
{
    if (!m_client)
        return 0;
    int n = 0;
    for (const OContact &c : m_client->ssiManager()->contactsFromGroup(name))
        if (c.type() == Oscar::ROSTER_CONTACT)
            ++n;
    return n;
}

bool AccountManager::addContact(const QString &rawUin, const QString &group, const QString &reason)
{
    QString uin = rawUin.trimmed();
    if (!uin.isEmpty() && uin.at(0).isDigit())
        uin.remove(QRegularExpression(QStringLiteral("[^0-9]")));
    if (uin.isEmpty() || !isOnline()) {
        emit contactEditFailed(uin, isOnline() ? tr("Enter an ICQ number or screen name.") : tr("Go online to change your contact list."));
        return false;
    }
    if (normalized(uin) == normalized(m_uin)) {
        emit contactEditFailed(uin, tr("That is your own number."));
        return false;
    }
    if (isContact(uin)) {
        emit contactEditFailed(uin, tr("%1 is already in your contact list.").arg(displayName(uin)));
        return false;
    }
    QString target = group.trimmed();
    if (target.isEmpty())
        target = m_groups.isEmpty() ? tr("Buddies") : m_groups.first();
    if (!reason.isNull())
        m_authReasons.insert(normalized(uin), reason);
    else
        m_authReasons.insert(normalized(uin), tr("Please add me to your contact list."));
    if (!m_client->ssiManager()->findGroup(target)) {
        m_pendingAdds.append({ uin, target, reason });
        m_client->addGroup(target);
        return true;
    }
    m_client->addContact(uin, target);
    return true;
}

void AccountManager::flushPendingAdds()
{
    if (!m_client || m_pendingAdds.isEmpty())
        return;
    QVector<PendingAdd> still;
    for (const PendingAdd &p : m_pendingAdds) {
        if (m_client->ssiManager()->findGroup(p.group))
            m_client->addContact(p.uin, p.group);
        else
            still.append(p);
    }
    m_pendingAdds = still;
}

void AccountManager::removeContact(const QString &uin)
{
    if (!isOnline() || !isContact(uin)) {
        // Only a conversation: drop it from the list (history stays).
        if (!isContact(uin))
            m_contacts->remove(uin);
        return;
    }
    m_client->removeContact(Oscar::normalize(uin));
}

void AccountManager::renameContact(const QString &uin, const QString &alias)
{
    if (isOnline() && isContact(uin))
        m_client->changeContactAlias(Oscar::normalize(uin), alias.trimmed());
}

void AccountManager::moveContact(const QString &uin, const QString &group)
{
    const QString target = group.trimmed();
    if (!isOnline() || !isContact(uin) || target.isEmpty() || groupOf(uin) == target)
        return;
    if (!m_client->ssiManager()->findGroup(target))
        m_client->addGroup(target);
    // changeContactGroup needs the group to exist; it may still be on its way.
    QTimer *t = new QTimer(this);
    int *tries = new int(0);
    const QString key = Oscar::normalize(uin);
    connect(t, &QTimer::timeout, this, [this, t, tries, key, target]() {
        if (m_client && m_client->ssiManager()->findGroup(target)) {
            m_client->changeContactGroup(key, target);
            t->stop();
        } else if (++*tries > 40) {
            t->stop();
        }
        if (!t->isActive()) {
            delete tries;
            t->deleteLater();
        }
    });
    t->start(250);
}

bool AccountManager::addGroup(const QString &name)
{
    const QString n = name.trimmed();
    if (!isOnline() || n.isEmpty() || m_client->ssiManager()->findGroup(n))
        return false;
    m_client->addGroup(n);
    return true;
}

void AccountManager::renameGroup(const QString &oldName, const QString &newName)
{
    const QString n = newName.trimmed();
    if (isOnline() && !n.isEmpty() && n != oldName && m_client->ssiManager()->findGroup(oldName)
            && !m_client->ssiManager()->findGroup(n))
        m_client->renameGroup(oldName, n);
}

bool AccountManager::removeGroup(const QString &name)
{
    if (!isOnline() || !m_client->ssiManager()->findGroup(name) || groupSize(name) > 0)
        return false;
    m_client->removeGroup(name);
    return true;
}

void AccountManager::requestAuthorization(const QString &uin, const QString &reason)
{
    if (isOnline())
        m_client->requestAuth(Oscar::normalize(uin), reason.isEmpty() ? tr("Please add me to your contact list.") : reason);
}

void AccountManager::onContactEditFailed(const QString &contact, int code)
{
    m_authReasons.remove(normalized(contact));
    QString why;
    switch (code) {
    case 0x0C: why = tr("Your contact list is full."); break;
    case 0x0D: why = tr("ICQ and AIM contacts cannot be mixed in this list."); break;
    case 0x0E: why = tr("%1 requires authorization.").arg(contact); break;
    default: why = tr("The server did not accept the change (error %1).").arg(code);
    }
    emit contactEditFailed(contact, why);
}

void AccountManager::onContactAddedYou(const QString &contact)
{
    if (!isBlocked(contact))
        emit contactAddedYou(contact, displayName(contact));
}

// ---- privacy --------------------------------------------------------------

bool AccountManager::isBlocked(const QString &uin) const
{
    const QString key = normalized(uin);
    for (const QString &b : m_blocked)
        if (normalized(b) == key)
            return true;
    return false;
}

bool AccountManager::isVisibleTo(const QString &uin) const
{
    const QString key = normalized(uin);
    for (const QString &b : m_visibleTo)
        if (normalized(b) == key)
            return true;
    return false;
}

bool AccountManager::isInvisibleTo(const QString &uin) const
{
    const QString key = normalized(uin);
    for (const QString &b : m_invisibleTo)
        if (normalized(b) == key)
            return true;
    return false;
}

void AccountManager::block(const QString &uin)
{
    if (!isOnline() || uin.isEmpty() || normalized(uin) == normalized(m_uin))
        return;
    const QString key = Oscar::normalize(uin);
    m_client->setIgnore(key, true);
    m_client->setInvisibleTo(key, true);
    // The invisible list only counts in "everyone except" mode.
    if (m_privacyMode == PrivacyUnknown || m_privacyMode == AllowAll)
        m_client->setPrivacyTLVs(BlockInvisibleList);
}

void AccountManager::unblock(const QString &uin)
{
    if (!isOnline())
        return;
    const QString key = Oscar::normalize(uin);
    m_client->setIgnore(key, false);
    m_client->setInvisibleTo(key, false);
}

void AccountManager::setVisibleTo(const QString &uin, bool visible)
{
    if (isOnline())
        m_client->setVisibleTo(Oscar::normalize(uin), visible);
}

void AccountManager::setInvisibleTo(const QString &uin, bool invisible)
{
    if (isOnline())
        m_client->setInvisibleTo(Oscar::normalize(uin), invisible);
}

void AccountManager::setPrivacyMode(int mode)
{
    if (isOnline() && mode >= AllowAll && mode <= AllowContactList)
        m_client->setPrivacyTLVs(Oscar::BYTE(mode));
}

void AccountManager::setAuthorizationRequired(bool required)
{
    if (!isOnline() || !isIcq())
        return;
    m_client->setAuthorizationRequired(required);
    m_authRequired = required ? 1 : 0;
    emit ownInfoChanged();
}

// ---- directory ------------------------------------------------------------

void AccountManager::requestUserInfo(const QString &uin)
{
    if (!isOnline() || uin.isEmpty())
        return;
    if (uin.at(0).isDigit())
        m_client->requestFullInfo(uin);
}

void AccountManager::onShortInfo(const QString &contact)
{
    if (!m_client)
        return;
    const ICQShortInfo i = m_client->getShortInfo(contact);
    QVariantMap info = m_userInfo.value(normalized(contact));
    info.insert(QStringLiteral("uin"), contact);
    info.insert(QStringLiteral("nickname"), legacyText(i.nickname));
    info.insert(QStringLiteral("firstName"), legacyText(i.firstName));
    info.insert(QStringLiteral("lastName"), legacyText(i.lastName));
    info.insert(QStringLiteral("email"), legacyText(i.email));
    info.insert(QStringLiteral("authRequired"), i.needsAuth.get());
    m_userInfo.insert(normalized(contact), info);
    if (normalized(contact) == normalized(m_uin)) {
        m_nickname = legacyText(i.nickname);
        m_authRequired = i.needsAuth.get() ? 1 : 0;
        emit ownInfoChanged();
    }
    emit userInfoReceived(contact, info);
}

void AccountManager::onLongInfo(const QString &contact)
{
    if (!m_client)
        return;
    const ICQGeneralUserInfo g = m_client->getGeneralInfo(contact);
    const ICQMoreUserInfo m = m_client->getMoreInfo(contact);
    const ICQWorkUserInfo w = m_client->getWorkInfo(contact);
    const ICQNotesInfo n = m_client->getNotesInfo(contact);
    QVariantMap info;
    info.insert(QStringLiteral("uin"), contact);
    info.insert(QStringLiteral("nickname"), legacyText(g.nickName.get()));
    info.insert(QStringLiteral("firstName"), legacyText(g.firstName.get()));
    info.insert(QStringLiteral("lastName"), legacyText(g.lastName.get()));
    info.insert(QStringLiteral("email"), g.publishEmail.get() ? legacyText(g.email.get()) : QString());
    info.insert(QStringLiteral("city"), legacyText(g.city.get()));
    info.insert(QStringLiteral("state"), legacyText(g.state.get()));
    info.insert(QStringLiteral("country"), countryName(int(g.country.get())));
    info.insert(QStringLiteral("authRequired"), g.needsAuth.get());
    info.insert(QStringLiteral("webAware"), g.webAware.get());
    info.insert(QStringLiteral("age"), m.age.get() > 0 && m.age.get() < 130 ? m.age.get() : 0);
    info.insert(QStringLiteral("gender"), int(m.gender.get())); // 1 female, 2 male
    info.insert(QStringLiteral("homepage"), legacyText(m.homepage.get()));
    if (m.birthdayYear.get() > 0 && m.birthdayMonth.get() > 0 && m.birthdayDay.get() > 0)
        info.insert(QStringLiteral("birthday"), QDate(m.birthdayYear.get(), m.birthdayMonth.get(), m.birthdayDay.get()));
    info.insert(QStringLiteral("company"), legacyText(w.company.get()));
    info.insert(QStringLiteral("position"), legacyText(w.position.get()));
    info.insert(QStringLiteral("about"), legacyText(n.notes.get()));
    m_userInfo.insert(normalized(contact), info);
    if (normalized(contact) == normalized(m_uin)) {
        m_nickname = info.value(QStringLiteral("nickname")).toString();
        m_authRequired = g.needsAuth.get() ? 1 : 0;
        emit ownInfoChanged();
    }
    emit userInfoReceived(contact, info);
}

bool AccountManager::startSearch()
{
    if (!isOnline() || !isIcq())
        return false;
    m_search->clear();
    m_searching = true;
    emit searchingChanged();
    m_searchTimer.start();
    return true;
}

void AccountManager::finishSearch()
{
    m_searchTimer.stop();
    if (!m_searching)
        return;
    m_searching = false;
    emit searchingChanged();
    emit searchFinished(m_search->rowCount());
}

void AccountManager::clearSearch()
{
    finishSearch();
    m_search->clear();
}

bool AccountManager::searchByUin(const QString &rawUin)
{
    QString uin = rawUin;
    uin.remove(QRegularExpression(QStringLiteral("[^0-9]")));
    if (uin.isEmpty() || !startSearch())
        return false;
    m_client->uinSearch(uin);
    return true;
}

bool AccountManager::searchByNickname(const QString &nickname)
{
    if (nickname.trimmed().isEmpty() || !startSearch())
        return false;
    ICQWPSearchInfo info;
    info.nickName = nickname.trimmed().toUtf8();
    m_client->whitePagesSearch(info);
    return true;
}

bool AccountManager::searchByName(const QString &firstName, const QString &lastName)
{
    if ((firstName.trimmed().isEmpty() && lastName.trimmed().isEmpty()) || !startSearch())
        return false;
    ICQWPSearchInfo info;
    info.firstName = firstName.trimmed().toUtf8();
    info.lastName = lastName.trimmed().toUtf8();
    m_client->whitePagesSearch(info);
    return true;
}

bool AccountManager::searchByEmail(const QString &email)
{
    if (!email.contains(QLatin1Char('@')) || !startSearch())
        return false;
    ICQWPSearchInfo info;
    info.email = email.trimmed().toUtf8();
    m_client->whitePagesSearch(info);
    return true;
}

void AccountManager::onSearchResult(const ICQSearchResult &r)
{
    if (!m_searching || r.uin == 0)
        return;
    SearchModel::Item it;
    it.uin = QString::number(r.uin);
    it.nickname = legacyText(r.nickName);
    it.firstName = legacyText(r.firstName);
    it.lastName = legacyText(r.lastName);
    it.email = legacyText(r.email);
    it.online = r.online;
    it.authRequired = r.auth;
    it.gender = r.gender == 'F' || r.gender == 1 ? 1 : (r.gender == 'M' || r.gender == 2 ? 2 : 0);
    it.age = r.age;
    m_search->add(it);
    m_searchTimer.start(); // more may follow
}

void AccountManager::onSearchEnd(int)
{
    finishSearch();
}

// ---- files ----------------------------------------------------------------

QString AccountManager::fileProxyHost() const
{
    if (m_profile == QLatin1String("custom"))
        return m_host;
    return QString::fromLatin1(kNinaProxyHost);
}

void AccountManager::setMessageSound(int sound)
{
    if (sound < 0 || sound > 2 || sound == m_messageSound)
        return;
    if (sound == 2 && !QFileInfo::exists(m_customSoundFile))
        return;
    m_messageSound = sound;
    saveSettings();
    emit soundChanged();
}

bool AccountManager::setCustomSound(const QString &pathOrUrl)
{
    const QString path = pathOrUrl.startsWith(QLatin1String("file:")) ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile() || !info.isReadable() || info.size() > 5 * 1024 * 1024)
        return false;
    const QString dir = m_dataDir + QStringLiteral("/sounds");
    QDir().mkpath(dir);
    const QString suffix = info.suffix().isEmpty() ? QStringLiteral("wav") : info.suffix().toLower();
    // A new name each time, so the notification service never plays a cached copy.
    const QString target = QStringLiteral("%1/message-%2.%3").arg(dir).arg(QDateTime::currentMSecsSinceEpoch()).arg(suffix);
    if (!QFile::copy(path, target))
        return false;
    if (!m_customSoundFile.isEmpty() && m_customSoundFile != target)
        QFile::remove(m_customSoundFile);
    m_customSoundFile = target;
    m_customSoundName = info.fileName();
    m_messageSound = 2;
    saveSettings();
    emit soundChanged();
    return true;
}

void AccountManager::setProxyTransfers(bool proxy)
{
    if (m_proxyTransfers == proxy)
        return;
    m_proxyTransfers = proxy;
    saveSettings();
    applyFileSettings();
    emit proxyTransfersChanged();
}

void AccountManager::setFileProxyOverride(const QString &host, int port)
{
    m_proxyOverrideHost = host;
    m_proxyOverridePort = port;
    applyFileSettings();
}

void AccountManager::applyFileSettings()
{
    if (!m_client)
        return;
    Oscar::Settings *s = m_client->clientSettings();
    s->setFileProxy(m_proxyTransfers);
    s->setFileProxyHost(m_proxyOverrideHost.isEmpty() ? fileProxyHost() : m_proxyOverrideHost);
    s->setFileProxyPort(m_proxyOverridePort > 0 ? m_proxyOverridePort : 5190);
    s->setTimeout(15); // seconds per connection attempt
}

void AccountManager::onIncomingFile(FileTransferHandler *handler)
{
    m_transfers->handleIncoming(handler);
}

qint64 AccountManager::addFileMessage(StoredMessage m)
{
    if (m.text.isEmpty())
        m.text = QStringLiteral("\U0001F4CE ") + m.fileName; // paperclip
    const bool incoming = !m.outgoing;
    const bool seen = !incoming || (m_appActive && normalized(m_activeChat) == normalized(m.contact));
    m.read = seen;
    m.id = m_history.addMessage(m);
    if (ChatModel *c = m_chats.value(normalized(m.contact)))
        c->append(m);
    if (!m_contacts->contains(m.contact)) {
        ContactModel::Item it;
        it.uin = m.contact;
        m_contacts->upsert(it);
    }
    m_contacts->setLastMessage(m.contact, m.text, m.timestamp);
    if (!seen)
        m_contacts->setUnread(m.contact, m_history.unreadCount(m.contact));
    return m.id;
}

void AccountManager::updateFileMessage(const QString &uin, qint64 id, int state, const QString &path, const QString &url)
{
    m_history.updateFile(id, state, path, url);
    if (ChatModel *c = m_chats.value(normalized(uin)))
        c->updateFile(id, state, path, url);
}

void AccountManager::setFileMessageProgress(const QString &uin, qint64 id, qint64 bytes)
{
    if (ChatModel *c = m_chats.value(normalized(uin)))
        c->setFileDone(id, bytes);
}

