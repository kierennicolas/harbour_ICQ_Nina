import QtQuick 2.6
import Sailfish.Silica 1.0

// Add someone to the server-side contact list. If they require
// authorization, the message goes with the request.
Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string uin
    property string name
    property bool authRequired: false

    readonly property bool creating: account.groups.length === 0 || groupBox.currentIndex === account.groups.length
    readonly property string group: creating ? (newGroup.text.trim().length > 0 ? newGroup.text.trim() : qsTr("Buddies"))
                                             : account.groups[groupBox.currentIndex]

    canAccept: account.state === 2

    onAccepted: account.addContact(uin, group, reason.text.trim())

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            DialogHeader {
                title: qsTr("Add contact")
                acceptText: qsTr("Add")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                text: dialog.name.length > 0 && dialog.name !== dialog.uin ? dialog.name : dialog.uin
            }
            Label {
                visible: dialog.name.length > 0 && dialog.name !== dialog.uin
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                color: Theme.secondaryHighlightColor
                text: dialog.uin
            }

            Item { width: 1; height: Theme.paddingLarge }

            ComboBox {
                id: groupBox
                visible: account.groups.length > 0
                label: qsTr("Group")
                menu: ContextMenu {
                    Repeater {
                        model: account.groups
                        MenuItem { text: modelData }
                    }
                    MenuItem { text: qsTr("New group…") }
                }
            }

            TextField {
                id: newGroup
                visible: dialog.creating
                width: parent.width
                label: qsTr("New group name")
                placeholderText: qsTr("Buddies")
            }

            TextArea {
                id: reason
                width: parent.width
                label: dialog.authRequired ? qsTr("Message with your request (they must approve)")
                                           : qsTr("Message if they ask to approve")
                placeholderText: qsTr("Please add me to your contact list.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: dialog.authRequired
                      ? qsTr("%1 approves who may add them. They appear as waiting for authorization until they answer.").arg(dialog.name.length > 0 ? dialog.name : dialog.uin)
                      : qsTr("If they turn out to require authorization, the request is sent automatically.")
            }
        }

        VerticalScrollDecorator {}
    }
}
