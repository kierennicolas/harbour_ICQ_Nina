import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../components/Presence.js" as Presence

// A contact's details from the ICQ directory, and what you can do with them:
// rename, move, ask for authorization, block, hide from, remove.
Page {
    id: page
    allowedOrientations: Orientation.All

    property string uin
    property var info: ({})
    property bool loading: false
    property bool isContact: false
    property bool blocked: false
    property bool visibleTo: false
    property bool invisibleTo: false
    property int presence: Presence.Offline
    property bool awaitingAuth: false
    property string name: uin
    property string group

    function refresh() {
        isContact = account.isContact(uin)
        blocked = account.isBlocked(uin)
        visibleTo = account.isVisibleTo(uin)
        invisibleTo = account.isInvisibleTo(uin)
        presence = account.contacts.presenceOf(uin)
        name = account.displayName(uin)
        group = account.groupOf(uin)
        awaitingAuth = account.contacts.isAwaitingAuth(uin)
    }

    function value(key) {
        var v = info[key]
        return v === undefined || v === null ? "" : String(v)
    }

    function rename() {
        var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
            title: qsTr("Rename"), label: qsTr("Name in your list"),
            value: account.aliasOf(uin), placeholder: page.value("nickname") || uin, allowEmpty: true })
        d.accepted.connect(function() { account.renameContact(uin, d.value) })
    }

    function move() {
        var d = pageStack.push(Qt.resolvedUrl("GroupDialog.qml"), { current: group })
        d.accepted.connect(function() { account.moveContact(uin, d.group) })
    }

    Component.onCompleted: {
        refresh()
        info = account.userInfo(uin)
        if (account.state === 2 && account.isIcq && uin.length > 0 && /^[0-9]+$/.test(uin)) {
            loading = true
            loadTimer.start()
            account.requestUserInfo(uin)
        }
    }

    Timer {
        id: loadTimer
        interval: 15000
        onTriggered: page.loading = false
    }

    Connections {
        target: account
        onUserInfoReceived: {
            if (uin.replace(/ /g, "").toLowerCase() === page.uin.replace(/ /g, "").toLowerCase()) {
                page.info = info
                page.loading = false
            }
        }
        onPrivacyChanged: page.refresh()
        onGroupsChanged: page.refresh()
    }
    Connections {
        target: account.contacts
        onModelReset: page.refresh()
        onDataChanged: page.refresh()
        onPresenceChanged: page.refresh()
    }

    RemorsePopup { id: remorse }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                visible: page.isContact && account.state === 2
                text: qsTr("Remove from contact list")
                onClicked: remorse.execute(qsTr("Removing %1").arg(page.name), function() {
                    account.removeContact(page.uin)
                })
            }
            MenuItem {
                visible: page.isContact && account.state === 2
                text: qsTr("Move to group")
                onClicked: page.move()
            }
            MenuItem {
                visible: page.isContact && account.state === 2
                text: qsTr("Rename")
                onClicked: page.rename()
            }
            MenuItem {
                visible: !page.isContact && account.state === 2
                text: qsTr("Add to contact list")
                onClicked: pageStack.push(Qt.resolvedUrl("AddContactDialog.qml"), {
                    uin: page.uin, name: page.value("nickname"), authRequired: page.info["authRequired"] === true })
            }
            MenuItem {
                text: qsTr("Send message")
                onClicked: {
                    var prev = pageStack.previousPage()
                    if (prev && prev.objectName === "conversation" && prev.uin === page.uin)
                        pageStack.pop()
                    else
                        pageStack.push(Qt.resolvedUrl("ConversationPage.qml"), { uin: page.uin })
                }
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader {
                title: page.name
                description: page.name !== page.uin ? page.uin : ""
            }

            Row {
                x: Theme.horizontalPageMargin
                spacing: Theme.paddingMedium
                height: Theme.itemSizeExtraSmall
                PresenceDot {
                    presence: page.presence
                    anchors.verticalCenter: parent.verticalCenter
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.highlightColor
                    text: Presence.name(page.presence)
                          + (page.isContact ? (page.group.length > 0 ? " · " + page.group : "")
                                            : " · " + qsTr("not in your list"))
                }
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: page.loading
                visible: running
                size: BusySize.Medium
            }

            SectionHeader {
                text: qsTr("Details")
                visible: !page.loading
            }

            DetailItem { label: qsTr("Nickname"); value: page.value("nickname"); visible: value.length > 0 }
            DetailItem {
                label: qsTr("Name")
                value: (page.value("firstName") + " " + page.value("lastName")).trim()
                visible: value.length > 0
            }
            DetailItem { label: qsTr("Email"); value: page.value("email"); visible: value.length > 0 }
            DetailItem {
                label: qsTr("Location")
                value: [page.value("city"), page.value("state"), page.value("country")].filter(function(s) { return s.length > 0 }).join(", ")
                visible: value.length > 0
            }
            DetailItem { label: qsTr("Age"); value: page.info["age"] > 0 ? String(page.info["age"]) : ""; visible: value.length > 0 }
            DetailItem {
                label: qsTr("Gender")
                value: page.info["gender"] === 1 ? qsTr("Female") : page.info["gender"] === 2 ? qsTr("Male") : ""
                visible: value.length > 0
            }
            DetailItem {
                label: qsTr("Birthday")
                value: page.info["birthday"] ? Format.formatDate(page.info["birthday"], Formatter.DateLong) : ""
                visible: value.length > 0
            }
            DetailItem { label: qsTr("Home page"); value: page.value("homepage"); visible: value.length > 0 }
            DetailItem {
                label: qsTr("Work")
                value: [page.value("position"), page.value("company")].filter(function(s) { return s.length > 0 }).join(", ")
                visible: value.length > 0
            }
            DetailItem {
                label: qsTr("Adding them")
                value: page.info["authRequired"] === true ? qsTr("Needs their approval") : qsTr("Open")
                visible: page.info["authRequired"] !== undefined
            }

            Label {
                visible: page.value("about").length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: page.value("about")
            }

            Label {
                visible: !page.loading && Object.keys(page.info).length <= 1
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: account.state !== 2 ? qsTr("Go online to see their details.")
                    : !account.isIcq ? qsTr("Details are only available for ICQ numbers.")
                    : qsTr("No details in the directory.")
            }

            SectionHeader {
                text: qsTr("Authorization")
                visible: page.isContact && account.isIcq
            }

            Label {
                visible: page.isContact && account.isIcq
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: page.awaitingAuth ? qsTr("Waiting for %1 to approve you. Until then you may not see when they are online.").arg(page.name)
                                        : qsTr("You are on their list.")
            }

            Button {
                visible: page.isContact && account.isIcq && account.state === 2 && page.awaitingAuth
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Ask again")
                onClicked: {
                    var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
                        title: qsTr("Ask for authorization"), label: qsTr("Message"),
                        value: qsTr("Please add me to your contact list."), acceptText: qsTr("Send") })
                    d.accepted.connect(function() { account.requestAuthorization(page.uin, d.value) })
                }
            }

            SectionHeader { text: qsTr("Privacy") }

            TextSwitch {
                text: qsTr("Blocked")
                description: qsTr("No messages, files or requests from them, and they cannot see you online.")
                checked: page.blocked
                enabled: account.state === 2
                automaticCheck: false
                onClicked: page.blocked ? account.unblock(page.uin) : account.block(page.uin)
            }
            TextSwitch {
                text: qsTr("Always show me online to them")
                description: qsTr("Even when your status is invisible.")
                checked: page.visibleTo
                enabled: account.state === 2 && !page.blocked
                automaticCheck: false
                onClicked: account.setVisibleTo(page.uin, !page.visibleTo)
            }
            TextSwitch {
                text: qsTr("Never show me online to them")
                description: qsTr("They can still message you.")
                checked: page.invisibleTo
                enabled: account.state === 2 && !page.blocked
                automaticCheck: false
                onClicked: account.setInvisibleTo(page.uin, !page.invisibleTo)
            }
        }

        VerticalScrollDecorator {}
    }
}
