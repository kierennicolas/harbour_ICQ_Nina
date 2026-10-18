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

#include "contactmodel.h"
#include "historystore.h"

class ChatModel;
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

    // configDir: where account.conf lives; dataDir: where history.sqlite lives.
    AccountManager(const QString &configDir, const QString &dataDir, QObject *parent = nullptr);
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
};

#endif
