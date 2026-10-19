import QtQuick 2.6
import Sailfish.Silica 1.0

// The groups of the server-side contact list: add, rename, delete (empty
// groups only).
Page {
    id: page
    allowedOrientations: Orientation.All

    // Rows are recreated when the groups change, so actions that finish later
    // (a dialog, the undo countdown) are set up here on the page.
    RemorsePopup { id: remorse }

    function renameGroup(name) {
        var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
            title: qsTr("Rename group"), label: qsTr("Group name"), value: name })
        d.accepted.connect(function() { account.renameGroup(name, d.value) })
    }

    function deleteGroup(name) {
        remorse.execute(qsTr("Deleting %1").arg(name), function() { account.removeGroup(name) })
    }

    function newGroup() {
        var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
            title: qsTr("New group"), label: qsTr("Group name"), acceptText: qsTr("Create") })
        d.accepted.connect(function() { account.addGroup(d.value) })
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: account.groups

        header: PageHeader { title: qsTr("Groups") }

        PullDownMenu {
            enabled: account.state === 2
            MenuItem {
                text: qsTr("New group")
                onClicked: page.newGroup()
            }
        }

        delegate: ListItem {
            id: item
            readonly property int size: { account.groups; return account.groupSize(modelData) }
            contentHeight: Theme.itemSizeSmall
            enabled: account.state === 2
            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Rename")
                    onClicked: page.renameGroup(modelData)
                }
                MenuItem {
                    text: item.size > 0 ? qsTr("Delete (move its contacts first)") : qsTr("Delete")
                    enabled: item.size === 0
                    onClicked: page.deleteGroup(modelData)
                }
            }
            onClicked: openMenu()

            Label {
                x: Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 2 * x - countLabel.width
                truncationMode: TruncationMode.Fade
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                text: modelData
            }
            Label {
                id: countLabel
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                color: Theme.secondaryColor
                text: item.size
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0
            text: qsTr("No groups")
            hintText: account.state === 2 ? qsTr("Pull down to create one") : qsTr("Go online to see your groups")
        }

        VerticalScrollDecorator {}
    }
}
