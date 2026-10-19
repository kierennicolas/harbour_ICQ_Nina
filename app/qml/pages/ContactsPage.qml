import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../components/Presence.js" as Presence

Page {
    id: page
    objectName: "contactsPage"
    allowedOrientations: Orientation.All

    property bool askedForAccount: false
    property int tick: 0 // re-evaluates the countdown text once a second

    // The long-press menu is created when it opens and destroyed when it
    // closes, and list updates recreate rows, so anything that runs later
    // (after the undo countdown or a dialog) must be set up here, on the
    // page, not in the menu or the row.
    RemorsePopup { id: removeRemorse }

    function removeEntry(uin, name, listed) {
        removeRemorse.execute(listed ? qsTr("Removing %1").arg(name) : qsTr("Deleting conversation with %1").arg(name),
                              function() {
                                  if (listed)
                                      account.removeContact(uin)
                                  else
                                      account.deleteConversation(uin)
                              })
    }

    function renameEntry(uin) {
        var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
            title: qsTr("Rename"), label: qsTr("Name in your list"),
            value: account.aliasOf(uin), placeholder: uin, allowEmpty: true })
        d.accepted.connect(function() { account.renameContact(uin, d.value) })
    }

    function moveEntry(uin) {
        var d = pageStack.push(Qt.resolvedUrl("GroupDialog.qml"), { current: account.groupOf(uin) })
        d.accepted.connect(function() { account.moveContact(uin, d.group) })
    }

    function connectionText() {
        if (!account.hasAccount)
            return qsTr("Not signed in")
        if (account.state === 2)
            return Presence.name(account.status) + (account.statusMessage.length > 0 ? " · " + account.statusMessage : "")
        if (account.state === 1)
            return qsTr("Connecting…")
        if (account.reconnectSeconds > 0)
            return qsTr("Offline · retrying in %1 s").arg(account.reconnectSeconds)
        return qsTr("Offline")
    }

    onStatusChanged: {
        if (status === PageStatus.Active && !account.hasAccount && !askedForAccount)
            signInTimer.start()
    }

    // Pushing from inside onStatusChanged re-enters the page's own status
    // change (a binding loop on "status"), so push once it has settled.
    Timer {
        id: signInTimer
        interval: 50
        onTriggered: {
            if (page.status !== PageStatus.Active || account.hasAccount || page.askedForAccount)
                return
            if (pageStack.busy) {
                restart()
                return
            }
            page.askedForAccount = true
            pageStack.push(Qt.resolvedUrl("SignInDialog.qml"))
        }
    }

    // Refresh the "retrying in N s" countdown.
    Timer {
        interval: 1000
        repeat: true
        running: page.status === PageStatus.Active && account.reconnectSeconds > 0
        onTriggered: page.tick++
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: account.contacts

        header: PageHeader {
            title: account.hasAccount ? account.uin : qsTr("ICQ Nina")
            description: { page.tick; return page.connectionText() }
        }

        PullDownMenu {
            busy: account.state === 1
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
            MenuItem {
                visible: account.hasAccount && account.state === 2
                text: qsTr("Add contact")
                onClicked: pageStack.push(Qt.resolvedUrl("AddContactPage.qml"))
            }
            MenuItem {
                visible: !account.hasAccount
                text: qsTr("Sign in")
                onClicked: pageStack.push(Qt.resolvedUrl("SignInDialog.qml"))
            }
            MenuItem {
                visible: account.hasAccount
                text: account.state === 0 ? qsTr("Go online") : qsTr("Status: %1").arg(Presence.name(account.status))
                onClicked: {
                    if (account.state === 0 && account.hasPassword)
                        account.connectAccount()
                    else if (account.state === 0)
                        pageStack.push(Qt.resolvedUrl("SignInDialog.qml"), { uin: account.uin })
                    else
                        pageStack.push(Qt.resolvedUrl("StatusPage.qml"))
                }
            }
        }

        section.property: "group"
        section.delegate: SectionHeader {
            text: section.length > 0 ? section : qsTr("Not in your contact list")
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeMedium
            onClicked: pageStack.push(Qt.resolvedUrl("ConversationPage.qml"), { uin: model.uin })
            menu: contactMenu


            Component {
                id: contactMenu
                ContextMenu {
                    readonly property bool listed: account.isContact(model.uin)
                    readonly property bool online: account.state === 2
                    MenuItem {
                        text: qsTr("Contact details")
                        onClicked: pageStack.push(Qt.resolvedUrl("ContactInfoPage.qml"), { uin: model.uin })
                    }
                    MenuItem {
                        visible: listed && online
                        text: qsTr("Rename")
                        onClicked: page.renameEntry(model.uin)
                    }
                    MenuItem {
                        visible: listed && online
                        text: qsTr("Move to group")
                        onClicked: page.moveEntry(model.uin)
                    }
                    MenuItem {
                        visible: !listed && online
                        text: qsTr("Add to contact list")
                        onClicked: pageStack.push(Qt.resolvedUrl("AddContactDialog.qml"), { uin: model.uin, name: model.name })
                    }
                    MenuItem {
                        visible: online
                        text: account.isBlocked(model.uin) ? qsTr("Unblock") : qsTr("Block")
                        onClicked: account.isBlocked(model.uin) ? account.unblock(model.uin) : account.block(model.uin)
                    }
                    MenuItem {
                        visible: !listed || online
                        text: listed ? qsTr("Remove from contact list") : qsTr("Delete conversation")
                        onClicked: page.removeEntry(model.uin, model.name, listed)
                    }
                }
            }

            Avatar {
                id: dot
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                size: Theme.iconSizeMedium
                source: model.icon || ""
                name: model.name
                uin: model.uin
                presence: model.presence
            }

            Column {
                anchors {
                    left: dot.right
                    leftMargin: Theme.paddingMedium
                    right: unreadBadge.visible ? unreadBadge.left : parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    text: model.name
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    font.bold: model.unread > 0
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    maximumLineCount: 1
                    wrapMode: Text.NoWrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: model.typing ? Theme.highlightColor
                         : (item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor)
                    textFormat: Text.StyledText
                    linkColor: color
                    // One line: newlines become spaces, emoji pictures.
                    text: model.typing ? qsTr("typing…")
                        : model.awaitingAuth ? qsTr("Waiting for authorization")
                        : model.lastMessage.length > 0 ? emoji.richText(model.lastMessage.replace(/\s+/g, " "), Math.round(font.pixelSize * 1.1))
                        : model.statusMessage.length > 0 ? emoji.richText(model.statusMessage, Math.round(font.pixelSize * 1.1))
                        : Presence.name(model.presence)
                }
            }

            Rectangle {
                id: unreadBadge
                visible: model.unread > 0
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                height: Theme.iconSizeExtraSmall + Theme.paddingSmall
                width: Math.max(height, unreadLabel.implicitWidth + Theme.paddingMedium)
                radius: height / 2
                color: Theme.highlightBackgroundColor
                Label {
                    id: unreadLabel
                    anchors.centerIn: parent
                    text: model.unread
                    font.pixelSize: Theme.fontSizeExtraSmall
                    font.bold: true
                    color: Theme.primaryColor
                }
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: !account.hasAccount ? qsTr("Not signed in")
                : account.state === 2 ? qsTr("No contacts yet")
                : qsTr("Your contacts appear here once you are online")
            hintText: !account.hasAccount ? qsTr("Pull down to sign in with your ICQ number")
                : account.lastError.length > 0 ? account.lastError
                : account.state === 2 ? qsTr("Pull down to add someone by ICQ number")
                : qsTr("Pull down to go online")
        }

        VerticalScrollDecorator {}
    }
}
