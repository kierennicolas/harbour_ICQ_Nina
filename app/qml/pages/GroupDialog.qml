import QtQuick 2.6
import Sailfish.Silica 1.0

// Pick one of the server-side groups, or name a new one. The caller reads
// group after accepted().
Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string title: qsTr("Move to group")
    property string current
    readonly property bool creating: groupBox.currentIndex === account.groups.length
    readonly property string group: creating ? newGroup.text.trim()
                                             : (account.groups[groupBox.currentIndex] || "")

    canAccept: group.length > 0 && group !== current

    Column {
        width: parent.width

        DialogHeader {
            title: dialog.title
            acceptText: qsTr("Move")
        }

        ComboBox {
            id: groupBox
            label: qsTr("Group")
            currentIndex: Math.max(0, account.groups.indexOf(dialog.current))
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
            placeholderText: label
        }
    }
}
