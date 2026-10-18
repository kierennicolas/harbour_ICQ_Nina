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
            if (Qt.application.state === Qt.ApplicationActive)
                pageStack.push(Qt.resolvedUrl("pages/AuthorizationDialog.qml"), { uin: uin, name: name, reason: reason })
        }
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
