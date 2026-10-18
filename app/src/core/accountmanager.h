// One ICQ/AIM account on an OSCAR server (NINA by default): sign-on,
// reconnection, status, contacts, messages and history. Pure Qt, so the
// same code runs in the Sailfish app and in desktop tests.
// Part of harbour_icq_nina; GPL-3.0-or-later.
#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include "contactmodel.h"
#include "historystore.h"
#include "searchmodel.h"
#include "transfermanager.h"

class ChatModel;
class FileTransferHandler;
class ICQSearchResult;
class QNetworkConfigurationManager;
class OContact;
class UserDetails;
namespace Oscar { class Client; class Message; struct SNAC; struct ClientVersion; }

class AccountManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString uin READ uin NOTIFY accountChanged)
    Q_PROPERTY(bool hasAccount READ hasAccount NOTIFY accountChanged)
    Q_PROPERTY(bool hasPassword READ hasPassword NOTIFY accountChanged)
    Q_PROPERTY(bool isIcq READ isIcq NOTIFY accountChanged)
    Q_PROPERTY(QString serverProfile READ serverProfile NOTIFY accountChanged)
    Q_PROPERTY(QString host READ host NOTIFY accountChanged)
    Q_PROPERTY(int port READ port NOTIFY accountChanged)
    Q_PROPERTY(bool tls READ tls NOTIFY accountChanged)
    Q_PROPERTY(int state READ state NOTIFY stateChanged)
    Q_PROPERTY(int status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int reconnectSeconds READ reconnectSeconds NOTIFY stateChanged)
    Q_PROPERTY(ContactModel *contacts READ contacts CONSTANT)
    Q_PROPERTY(int unreadTotal READ unreadTotal NOTIFY unreadTotalChanged)
    Q_PROPERTY(QString activeChat READ activeChat WRITE setActiveChat NOTIFY activeChatChanged)
    Q_PROPERTY(bool appActive READ appActive WRITE setAppActive NOTIFY appActiveChanged)
    // Keep the device from suspending while connected (costs battery, but
    // keeps the session reliably alive with the screen off).
    Q_PROPERTY(bool stayAwake READ stayAwake WRITE setStayAwake NOTIFY stayAwakeChanged)

    // Phase 2: contact list management, privacy, directory, files.
    Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
    Q_PROPERTY(QStringList blockedContacts READ blockedContacts NOTIFY privacyChanged)
    Q_PROPERTY(QStringList visibleList READ visibleList NOTIFY privacyChanged)
    Q_PROPERTY(QStringList invisibleList READ invisibleList NOTIFY privacyChanged)
    Q_PROPERTY(int privacyMode READ privacyMode NOTIFY privacyChanged)
    // -1 unknown (not read from the server yet), 0 anyone may add me, 1 they must ask.
    Q_PROPERTY(int authorizationRequired READ authorizationRequired NOTIFY ownInfoChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY ownInfoChanged)
    Q_PROPERTY(SearchModel *searchResults READ searchResults CONSTANT)
    Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
    Q_PROPERTY(TransferManager *transfers READ transfers CONSTANT)
    // Send files through the server's rendezvous proxy from the start (works
    // behind firewalls and on mobile data) instead of trying direct first.
    Q_PROPERTY(bool proxyTransfers READ proxyTransfers WRITE setProxyTransfers NOTIFY proxyTransfersChanged)
    Q_PROPERTY(QString fileProxyHost READ fileProxyHost NOTIFY accountChanged)

public:
    enum State { Offline = 0, Connecting, Online };
    Q_ENUM(State)

    // Same values as ContactModel::Presence.
    enum Status {
        StatusOnline = 0,
        StatusAway,
        StatusNotAvailable,
        StatusOccupied,
        StatusDoNotDisturb,
        StatusFreeForChat,
        StatusInvisible,
        StatusOffline
    };
    Q_ENUM(Status)

    // SSI privacy mode (TLV 0xCA of the permit/deny settings item).
    enum PrivacyMode {
        PrivacyUnknown = 0,
        AllowAll = 1,            // everyone can see me
        BlockAll = 2,            // nobody can see me
        AllowVisibleList = 3,    // only my "visible to" list
        BlockInvisibleList = 4,  // everyone except my "invisible to" list (ICQ default)
        AllowContactList = 5     // only my contact list
    };
    Q_ENUM(PrivacyMode)

    // configDir: where account.conf lives; dataDir: where history.sqlite lives.
    // downloadDir: where received files go (default: the Downloads folder).
    AccountManager(const QString &configDir, const QString &dataDir, QObject *parent = nullptr,
                   const QString &downloadDir = QString());
    ~AccountManager() override;

    QString uin() const { return m_uin; }
    bool hasAccount() const { return !m_uin.isEmpty(); }
    bool hasPassword() const { return !m_password.isEmpty(); }
    bool isIcq() const;
    QString serverProfile() const { return m_profile; }
    QString host() const { return m_host; }
    int port() const { return m_port; }
    bool tls() const { return m_tls; }
    int state() const { return m_state; }
    int status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }
    QString lastError() const { return m_lastError; }
    int reconnectSeconds() const;
    ContactModel *contacts() const { return m_contacts; }
    int unreadTotal() const;
    QString activeChat() const { return m_activeChat; }
    void setActiveChat(const QString &uin);
    bool appActive() const { return m_appActive; }
    void setAppActive(bool active);
    bool stayAwake() const { return m_stayAwake; }
    void setStayAwake(bool stay);

    QStringList groups() const { return m_groups; }
    QStringList blockedContacts() const { return m_blocked; }
    QStringList visibleList() const { return m_visibleTo; }
    QStringList invisibleList() const { return m_invisibleTo; }
    int privacyMode() const { return m_privacyMode; }
    int authorizationRequired() const { return m_authRequired; }
    QString nickname() const { return m_nickname; }
    SearchModel *searchResults() const { return m_search; }
    bool searching() const { return m_searching; }
    TransferManager *transfers() const { return m_transfers; }
    bool proxyTransfers() const { return m_proxyTransfers; }
    void setProxyTransfers(bool proxy);
    QString fileProxyHost() const;

    // profile: "nina-tls", "nina" or "custom" (then host/port/tls are used).
    Q_INVOKABLE void signIn(const QString &uin, const QString &password, bool rememberPassword,
                            const QString &profile = QStringLiteral("nina-tls"),
                            const QString &host = QString(), int port = 5190, bool tls = false);
    // Connect with the stored account (password may be supplied if not stored).
    Q_INVOKABLE void connectAccount(const QString &password = QString());
    Q_INVOKABLE void disconnectAccount();
    // Signs out and forgets the stored account and password (history stays).
    Q_INVOKABLE void forgetAccount();
    Q_INVOKABLE void setStatus(int status, const QString &message = QString());

    Q_INVOKABLE ChatModel *chat(const QString &uin);
    Q_INVOKABLE bool sendMessage(const QString &uin, const QString &text);
    Q_INVOKABLE void markRead(const QString &uin);
    Q_INVOKABLE void setTyping(const QString &uin, bool typing);
    Q_INVOKABLE void answerAuthorization(const QString &uin, bool grant);
    Q_INVOKABLE QString displayName(const QString &uin) const;

    // Contact list (server side). group may be empty: then the first group
    // (created if there is none). reason is sent if they require authorization.
    Q_INVOKABLE bool addContact(const QString &uin, const QString &group = QString(), const QString &reason = QString());
    Q_INVOKABLE void removeContact(const QString &uin);
    Q_INVOKABLE void renameContact(const QString &uin, const QString &alias);
    Q_INVOKABLE void moveContact(const QString &uin, const QString &group);
    Q_INVOKABLE bool addGroup(const QString &name);
    Q_INVOKABLE void renameGroup(const QString &oldName, const QString &newName);
    Q_INVOKABLE bool removeGroup(const QString &name); // only empty groups
    Q_INVOKABLE int groupSize(const QString &name) const;
    Q_INVOKABLE bool isContact(const QString &uin) const;
    Q_INVOKABLE QString groupOf(const QString &uin) const;
    Q_INVOKABLE QString aliasOf(const QString &uin) const;
    Q_INVOKABLE void requestAuthorization(const QString &uin, const QString &reason = QString());

    // Privacy. Blocking puts someone on the ignore list (no messages, files
    // or requests) and the invisible list (they cannot see you online).
    Q_INVOKABLE bool isBlocked(const QString &uin) const;
    Q_INVOKABLE void block(const QString &uin);
    Q_INVOKABLE void unblock(const QString &uin);
    Q_INVOKABLE bool isVisibleTo(const QString &uin) const;
    Q_INVOKABLE void setVisibleTo(const QString &uin, bool visible);
    Q_INVOKABLE bool isInvisibleTo(const QString &uin) const;
    Q_INVOKABLE void setInvisibleTo(const QString &uin, bool invisible);
    Q_INVOKABLE void setPrivacyMode(int mode);
    Q_INVOKABLE void setAuthorizationRequired(bool required);

    // ICQ directory. Results arrive in searchResults; userInfoReceived
    // carries a contact's details.
    Q_INVOKABLE void requestUserInfo(const QString &uin);
    Q_INVOKABLE QVariantMap userInfo(const QString &uin) const { return m_userInfo.value(normalized(uin)); }
    Q_INVOKABLE bool searchByUin(const QString &uin);
    Q_INVOKABLE bool searchByNickname(const QString &nickname);
    Q_INVOKABLE bool searchByName(const QString &firstName, const QString &lastName);
    Q_INVOKABLE bool searchByEmail(const QString &email);
    Q_INVOKABLE void clearSearch();

    // For TransferManager.
    bool isOnline() const { return m_state == Online && m_client; }
    bool sendText(const QString &uin, const QString &text, bool record);
    qint64 addFileMessage(StoredMessage m);
    void updateFileMessage(const QString &uin, qint64 id, int state, const QString &path, const QString &url);
    void setFileMessageProgress(const QString &uin, qint64 id, qint64 bytes);
    static QString normalized(const QString &uin);
    // Tests: use this rendezvous proxy instead of the profile's.
    void setFileProxyOverride(const QString &host, int port);

    // Called periodically (also while the device sleeps) to keep the
    // session alive and to detect a dead connection.
    Q_INVOKABLE void keepAlive();

    static QString presenceName(int presence);

signals:
    void accountChanged();
    void stateChanged();
    void statusChanged();
    void lastErrorChanged();
    void unreadTotalChanged();
    void activeChatChanged();
    void appActiveChanged();
    void stayAwakeChanged();
    // For notifications: someone wrote to us while we were not looking.
    void messageReceived(const QString &uin, const QString &name, const QString &text);
    void messageFailed(const QString &uin);
    void authorizationRequested(const QString &uin, const QString &name, const QString &reason);
    void authorizationAnswered(const QString &uin, bool granted);
    void groupsChanged();
    void privacyChanged();
    void ownInfoChanged();
    void searchingChanged();
    void searchFinished(int count);
    void userInfoReceived(const QString &uin, const QVariantMap &info);
    void contactEditFailed(const QString &uin, const QString &message);
    void contactAddedYou(const QString &uin, const QString &name);
    void proxyTransfersChanged();

private slots:
    void onLoggedIn();
    void onHaveContactList();
    void onHaveOwnInfo();
    void onTaskError(const Oscar::SNAC &snac, int code, bool fatal);
    void onSocketError(int code, const QString &message);
    void onDisconnected();
    void onConnectedElsewhere();
    void onMessage(const Oscar::Message &message);
    void onMessageAck(const QString &contact, uint id);
    void onMessageError(const QString &contact, uint id);
    void onUserInfo(const QString &contact, const UserDetails &details);
    void onUserOffline(const QString &contact);
    void onTypingStarted(const QString &contact);
    void onTypingStopped(const QString &contact);
    void onAuthRequest(const QString &contact, const QString &reason);
    void onAuthReply(const QString &contact, const QString &reason, bool granted);
    void onContactChanged(const OContact &item);
    void onContactRemoved(const QString &contact);
    void onGroupChanged();
    void onContactEditFailed(const QString &contact, int code);
    void onContactAddedYou(const QString &contact);
    void onShortInfo(const QString &contact);
    void onLongInfo(const QString &contact);
    void onSearchResult(const ICQSearchResult &result);
    void onSearchEnd(int usersLeft);
    void onIncomingFile(FileTransferHandler *handler);
    void onLivenessTimeout();
    void reconnect();

private:
    void loadSettings();
    void saveSettings();
    void openHistory();
    void setState(State s);
    void setError(const QString &error);
    void startClient();
    void dropClient();
    void connectionLost(const QString &why);
    void scheduleReconnect(int seconds);
    void rebuildContactsFromServer();
    void storeIncoming(const QString &uin, const QString &text, const QDateTime &when);
    QString groupName(int gid) const;
    void refreshGroupsAndPrivacy();
    void flushPendingAdds();
    bool startSearch();
    void finishSearch();
    void applyFileSettings();

    QString m_configDir;
    QString m_dataDir;
    QString m_uin;
    QString m_password;
    bool m_rememberPassword = true;
    QString m_profile = QStringLiteral("nina-tls");
    QString m_host;
    int m_port = 5190;
    bool m_tls = true;

    State m_state = Offline;
    int m_status = StatusOnline;      // wanted when online
    bool m_wantOnline = false;
    QString m_statusMessage;
    QString m_lastError;
    QString m_activeChat;
    bool m_appActive = true;
    bool m_stayAwake = false;

    Oscar::Client *m_client = nullptr;
    Oscar::ClientVersion *m_version = nullptr;
    ContactModel *m_contacts = nullptr;
    HistoryStore m_history;
    QHash<QString, QPointer<ChatModel>> m_chats;
    QHash<uint, QPair<QString, qint64>> m_pendingAcks; // OSCAR id -> (contact, history id)
    uint m_nextMessageId = 1;

    QTimer m_reconnectTimer;
    QDateTime m_reconnectAt;
    int m_retries = 0;
    QTimer m_livenessTimer;
    QNetworkConfigurationManager *m_network = nullptr;

    QStringList m_groups;
    QStringList m_blocked;      // ignore list
    QStringList m_visibleTo;
    QStringList m_invisibleTo;
    int m_privacyMode = PrivacyUnknown;
    int m_authRequired = -1;
    QString m_nickname;
    SearchModel *m_search = nullptr;
    bool m_searching = false;
    QTimer m_searchTimer;
    QHash<QString, QVariantMap> m_userInfo;
    struct PendingAdd { QString uin; QString group; QString reason; };
    QVector<PendingAdd> m_pendingAdds;          // waiting for their group to exist
    QHash<QString, QString> m_authReasons;      // added, will ask for authorization
    TransferManager *m_transfers = nullptr;
    bool m_proxyTransfers = true;
    QString m_proxyOverrideHost;
    int m_proxyOverridePort = 0;
};

#endif
