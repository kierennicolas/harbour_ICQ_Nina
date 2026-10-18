import QtQuick 2.6
import Sailfish.Silica 1.0

// Who can see you online, who must ask before adding you, and the blocked,
// "always visible to" and "never visible to" lists (all kept on the server).
Page {
    id: page
    allowedOrientations: Orientation.All

    // ComboBox order -> SSI privacy mode
    readonly property var modes: [4, 5, 3, 1, 2]

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Privacy and blocking") }

            Label {
                visible: account.state !== 2
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: qsTr("Go online to change these settings. They are stored on the server.")
            }

            ComboBox {
                label: qsTr("Who can see me online")
                enabled: account.state === 2
                currentIndex: Math.max(0, page.modes.indexOf(account.privacyMode))
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("Everyone except blocked people")
                        onClicked: account.setPrivacyMode(4)
                    }
                    MenuItem {
                        text: qsTr("Only my contacts")
                        onClicked: account.setPrivacyMode(5)
                    }
                    MenuItem {
                        text: qsTr("Only my \"always visible\" list")
                        onClicked: account.setPrivacyMode(3)
                    }
                    MenuItem {
                        text: qsTr("Everyone")
                        onClicked: account.setPrivacyMode(1)
                    }
                    MenuItem {
                        text: qsTr("Nobody")
                        onClicked: account.setPrivacyMode(2)
                    }
                }
            }
            Label {
                visible: account.privacyMode === 0 && account.state === 2
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Not set on the server yet, which means everyone except blocked people.")
            }

            TextSwitch {
                visible: account.isIcq
                text: qsTr("Others must ask before adding me")
                description: account.authorizationRequired < 0 ? qsTr("Checking the server…")
                           : qsTr("ICQ authorization: you approve each request.")
                checked: account.authorizationRequired === 1
                enabled: account.state === 2 && account.authorizationRequired >= 0
                automaticCheck: false
                onClicked: account.setAuthorizationRequired(account.authorizationRequired !== 1)
            }

            SectionHeader { text: qsTr("Blocked") }
            Label {
                visible: account.blockedContacts.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Nobody. Block someone from their contact details or from the contact list.")
            }
            Repeater {
                model: account.blockedContacts
                delegate: listEntry
            }

            SectionHeader {
                visible: account.visibleList.length > 0
                text: qsTr("Always see me online")
            }
            Repeater {
                model: account.visibleList
                delegate: ListItem {
                    contentHeight: Theme.itemSizeSmall
                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Remove from this list")
                            onClicked: account.setVisibleTo(modelData, false)
                        }
                    }
                    onClicked: openMenu()
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        anchors.verticalCenter: parent.verticalCenter
                        truncationMode: TruncationMode.Fade
                        text: account.displayName(modelData)
                    }
                }
            }

            SectionHeader {
                // Blocked people are on the invisible list too; show the others.
                visible: invisibleOnly.count > 0
                text: qsTr("Never see me online")
            }
            Repeater {
                id: invisibleOnly
                model: account.invisibleList.filter(function(u) { return account.blockedContacts.indexOf(u) < 0 })
                delegate: ListItem {
                    contentHeight: Theme.itemSizeSmall
                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Remove from this list")
                            onClicked: account.setInvisibleTo(modelData, false)
                        }
                    }
                    onClicked: openMenu()
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * x
                        anchors.verticalCenter: parent.verticalCenter
                        truncationMode: TruncationMode.Fade
                        text: account.displayName(modelData)
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }

    Component {
        id: listEntry
        ListItem {
            contentHeight: Theme.itemSizeSmall
            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Unblock")
                    onClicked: account.unblock(modelData)
                }
                MenuItem {
                    text: qsTr("Details")
                    onClicked: pageStack.push(Qt.resolvedUrl("ContactInfoPage.qml"), { uin: modelData })
                }
            }
            onClicked: openMenu()
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                anchors.verticalCenter: parent.verticalCenter
                truncationMode: TruncationMode.Fade
                text: account.displayName(modelData) + (account.displayName(modelData) !== modelData ? " · " + modelData : "")
            }
        }
    }
}
