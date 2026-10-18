import QtQuick 2.6
import Sailfish.Silica 1.0

// How to send a picked file: an OSCAR file transfer (the other person
// accepts it in their client) or a temporary download link sent as a
// message (works with every client, and when they are offline).
Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string uin
    property string name
    property string filePath
    property string fileName
    property real fileSize: 0
    property bool contactOnline: false

    // 0 direct, 1 link
    property int method: contactOnline ? 0 : 1
    readonly property var hours: [1, 12, 24, 72]

    canAccept: account.state === 2 && filePath.length > 0
               && (method === 1 ? fileSize <= 1000 * 1000 * 1000 : fileSize < 4294967296)

    onAccepted: {
        var id = method === 0 ? account.transfers.sendFile(uin, filePath)
                              : account.transfers.sendLink(uin, filePath, hours[expiry.currentIndex])
        if (id.length === 0)
            failed(account.transfers.lastError())
    }

    signal failed(string message)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column
            width: parent.width

            DialogHeader {
                title: qsTr("Send file")
                acceptText: qsTr("Send")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeLarge
                text: dialog.fileName
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                color: Theme.secondaryHighlightColor
                text: qsTr("%1 · to %2").arg(account.transfers.formatSize(dialog.fileSize)).arg(dialog.name)
            }

            Item { width: 1; height: Theme.paddingLarge }

            ComboBox {
                id: methodBox
                label: qsTr("Send as")
                currentIndex: dialog.method
                menu: ContextMenu {
                    MenuItem {
                        text: qsTr("File transfer")
                        onClicked: dialog.method = 0
                    }
                    MenuItem {
                        text: qsTr("Download link")
                        onClicked: dialog.method = 1
                    }
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: dialog.method === 0
                      ? (dialog.contactOnline
                         ? qsTr("%1 accepts the file in their ICQ or AIM client. Up to 4 GB.").arg(dialog.name)
                         : qsTr("%1 seems to be offline: a file transfer needs them online. A download link works now.").arg(dialog.name))
                      : qsTr("The file is uploaded to Litterbox (litterbox.catbox.moe) and the link is sent as a message. Anyone with the link can download it until it expires. Up to 1 GB.")
            }

            ComboBox {
                id: expiry
                visible: dialog.method === 1
                label: qsTr("Link works for")
                currentIndex: 3
                menu: ContextMenu {
                    MenuItem { text: qsTr("1 hour") }
                    MenuItem { text: qsTr("12 hours") }
                    MenuItem { text: qsTr("1 day") }
                    MenuItem { text: qsTr("3 days") }
                }
            }

            Label {
                visible: !dialog.canAccept && account.state === 2
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
                text: dialog.method === 1 ? qsTr("Too big for a link (1 GB at most).") : qsTr("Too big for a file transfer (4 GB at most).")
            }
        }

        VerticalScrollDecorator {}
    }
}
