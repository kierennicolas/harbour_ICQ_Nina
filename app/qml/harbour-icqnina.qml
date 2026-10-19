import QtQuick 2.6
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import Nemo.KeepAlive 1.2
import "pages"
import "cover"

ApplicationWindow {
    id: app

    // uin -> Notification shown for that conversation
    property var notifications: ({})
    // The new-message sound: empty for the phone's instant message tone.
    readonly property string messageSoundFile: account.messageSound === 1 ? Qt.resolvedUrl("../sounds/icq-nina-message.wav").toString()
                                             : account.messageSound === 2 ? "file://" + account.customSoundFile
                                             : ""

    // A short message over whatever page is open (a transient notification).
    function showError(text) {
        var n = errorNotificationComponent.createObject(app, { previewBody: text, body: text })
        n.publish()
    }

    function playMessageSound() {
        var n = testNotificationComponent.createObject(app)
        n.publish()
    }

    // Authorization requests not answered yet: [{uin, name, reason}]
    property var pendingAuthorizations: []

    function showNextAuthorization() {
        if (pendingAuthorizations.length === 0 || Qt.application.state !== Qt.ApplicationActive || pageStack.busy)
            return
        var r = pendingAuthorizations[0]
        pendingAuthorizations = pendingAuthorizations.slice(1)
        pageStack.push(Qt.resolvedUrl("pages/AuthorizationDialog.qml"), { uin: r.uin, name: r.name, reason: r.reason })
    }

    function isViewing(uin) {
        return Qt.application.state === Qt.ApplicationActive && account.activeChat === uin
    }

    initialPage: Component { ContactsPage {} }
    cover: Component { CoverPage {} }
    allowedOrientations: defaultAllowedOrientations

    function openConversation(uin) {
        var contactsPage = pageStack.find(function (p) { return p.objectName === "contactsPage" })
        if (contactsPage)
            pageStack.pop(contactsPage, PageStackAction.Immediate)
        pageStack.push(Qt.resolvedUrl("pages/ConversationPage.qml"), { uin: uin }, PageStackAction.Immediate)
        app.activate()
    }

    function notify(uin, name, text) {
        var n = notifications[uin]
        if (!n) {
            n = notificationComponent.createObject(app, { uin: uin })
            notifications[uin] = n
        }
        n.itemCount = n.itemCount + 1
        n.summary = name
        n.body = text
        n.previewSummary = name
        n.previewBody = text
        n.timestamp = new Date()
        n.publish()
    }

    function clearNotification(uin) {
        var n = notifications[uin]
        if (n) {
            n.close()
            n.destroy()
            delete notifications[uin]
        }
    }

    Component {
        id: notificationComponent
        Notification {
            property string uin
            category: "x-nemo.messaging.im"
            sound: app.messageSoundFile
            appName: qsTr("ICQ Nina")
            appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-icqnina.png"
            itemCount: 0
            remoteActions: [{
                "name": "default",
                "displayName": qsTr("Open"),
                "service": "io.github.kierennicolas.icqnina",
                "path": "/",
                "iface": "io.github.kierennicolas.icqnina",
                "method": "openConversation",
                "arguments": [ uin ]
            }]
        }
    }

    Component {
        id: errorNotificationComponent
        Notification {
            appName: qsTr("ICQ Nina")
            appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-icqnina.png"
            summary: qsTr("ICQ Nina")
            previewSummary: qsTr("ICQ Nina")
            isTransient: true
            onClosed: destroy()
        }
    }

    Component {
        id: testNotificationComponent
        Notification {
            category: "x-nemo.messaging.im"
            appName: qsTr("ICQ Nina")
            appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-icqnina.png"
            summary: qsTr("ICQ Nina")
            body: qsTr("This is how new messages sound")
            previewSummary: summary
            previewBody: body
            isTransient: true
            sound: app.messageSoundFile
            onClosed: destroy()
        }
    }

    Binding {
        target: account
        property: "appActive"
        value: Qt.application.state === Qt.ApplicationActive
    }

    Connections {
        target: account
        onMessageReceived: app.notify(uin, name, text)
        onAuthorizationRequested: {
            app.notify(uin, name, reason.length > 0
                       ? qsTr("Asks to add you to their list: %1").arg(reason)
                       : qsTr("Asks to add you to their list"))
            for (var i = 0; i < app.pendingAuthorizations.length; ++i)
                if (app.pendingAuthorizations[i].uin === uin)
                    return
            app.pendingAuthorizations = app.pendingAuthorizations.concat([{ uin: uin, name: name, reason: reason }])
            app.showNextAuthorization()
        }
        onAuthorizationAnswered: {
            app.notify(uin, account.displayName(uin), granted ? qsTr("Allowed you to add them")
                                                               : qsTr("Did not allow you to add them"))
        }
        onContactAddedYou: app.notify(uin, name, qsTr("Added you to their contact list"))
        onContactEditFailed: app.showError(message)
        onProfileSaved: app.showError(message.length > 0 ? message : ok ? qsTr("Your profile was saved") : qsTr("Not saved"))
    }

    Connections {
        target: account.transfers
        onIncomingOffer: {
            if (!app.isViewing(uin))
                app.notify(uin, account.displayName(uin), qsTr("Wants to send you %1 (%2)")
                           .arg(fileName).arg(account.transfers.formatSize(size)))
        }
        onTransferEnded: {
            // state 3 = done
            if (incoming && state === 3 && !app.isViewing(uin))
                app.notify(uin, account.displayName(uin), qsTr("Received %1").arg(fileName))
        }
    }

    Connections {
        target: Qt.application
        onStateChanged: app.showNextAuthorization()
    }

    Connections {
        target: pageStack
        onBusyChanged: if (!pageStack.busy) app.showNextAuthorization()
    }

    Connections {
        target: dbus
        onOpenConversationRequested: app.openConversation(uin)
        onActivateRequested: app.activate()
    }

    // OSCAR has no push: wake up regularly (also from suspend) so the
    // session stays alive and a dead connection is noticed and redialled.
    BackgroundJob {
        enabled: account.hasAccount && (account.state !== 0 || account.reconnectSeconds > 0)
        frequency: BackgroundJob.TwoAndHalfMinutes
        onTriggered: {
            account.keepAlive()
            finished()
        }
    }

    // Optional: do not suspend at all while connected.
    KeepAlive {
        enabled: account.stayAwake && account.state === 2
    }
}
