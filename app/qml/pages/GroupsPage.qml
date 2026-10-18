import QtQuick 2.6
import Sailfish.Silica 1.0

// The groups of the server-side contact list: add, rename, delete (empty
// groups only).
Page {
    id: page
    allowedOrientations: Orientation.All

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
                    onClicked: {
                        var name = modelData
                        var d = pageStack.push(Qt.resolvedUrl("TextInputDialog.qml"), {
                            title: qsTr("Rename group"), label: qsTr("Group name"), value: name })
                        d.accepted.connect(function() { account.renameGroup(name, d.value) })
                    }
                }
                MenuItem {
                    text: item.size > 0 ? qsTr("Delete (move its contacts first)") : qsTr("Delete")
                    enabled: item.size === 0
                    onClicked: {
                        var name = modelData
                        remorse.execute(item, qsTr("Deleting"), function() { account.removeGroup(name) })
                    }
                }
            }
            onClicked: openMenu()

            RemorseItem { id: remorse }

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
