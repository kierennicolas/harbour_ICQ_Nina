import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../components/Presence.js" as Presence

Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaListView {
        id: list
        anchors.fill: parent
        model: Presence.choices

        header: Column {
            width: list.width
            PageHeader { title: qsTr("Status") }
            TextField {
                id: messageField
                width: parent.width
                text: account.statusMessage
                label: qsTr("Status message")
                placeholderText: qsTr("Status message (optional)")
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: {
                    account.setStatus(account.status, text)
                    focus = false
                }
                Component.onCompleted: page.messageField = messageField
            }
        }

        delegate: ListItem {
            id: item
            readonly property int value: modelData
            highlighted: down || (value === account.status && account.state !== 0)
                         || (value === Presence.Offline && account.state === 0)
            onClicked: {
                var msg = page.messageField ? page.messageField.text : account.statusMessage
                account.setStatus(value, msg)
                pageStack.pop()
            }
            PresenceDot {
                id: dot
                presence: item.value
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
            }
            Label {
                anchors {
                    left: dot.right
                    leftMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                text: Presence.name(item.value)
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }
        VerticalScrollDecorator {}
    }

    property var messageField: null
}
