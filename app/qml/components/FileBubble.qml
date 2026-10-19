import QtQuick 2.6
import Sailfish.Silica 1.0

// A file in a conversation: name, size, state, progress and what you can do
// with it (accept, decline, cancel, open, copy link, send as a link instead).
Rectangle {
    id: bubble

    property bool outgoing
    property string fileName
    property real fileSize
    property int fileState
    property real fileDone
    property string filePath
    property string fileUrl
    property string transferId
    property var timestamp
    property string contactName
    property string contact
    property real maxWidth: parent ? parent.width * 0.85 : 400

    // TransferManager::State
    readonly property int stOffered: 0
    readonly property int stWaiting: 1
    readonly property int stTransferring: 2
    readonly property int stDone: 3
    readonly property int stFailed: 4
    readonly property int stCancelled: 5
    readonly property int stDeclined: 6
    readonly property int stUploading: 7
    readonly property bool isLink: transferId.indexOf("link-") === 0
    readonly property bool active: fileState === stOffered || fileState === stWaiting
                                   || fileState === stTransferring || fileState === stUploading
    readonly property real progress: fileSize > 0 ? Math.min(1, fileDone / fileSize) : 0

    signal message(string text)

    width: Math.min(maxWidth, Math.max(content.implicitWidth, Theme.itemSizeHuge * 2) + 2 * Theme.paddingMedium)
    height: content.height + 2 * Theme.paddingMedium
    radius: Theme.paddingMedium
    color: outgoing ? Theme.rgba(Theme.highlightBackgroundColor, 0.35) : Theme.rgba(Theme.secondaryColor, 0.15)

    function stateText() {
        var size = account.transfers.formatSize(fileSize)
        switch (fileState) {
        case stOffered:
            return outgoing ? size : qsTr("%1 · wants to send you this file").arg(size)
        case stWaiting:
            return outgoing ? qsTr("%1 · waiting for %2 to accept").arg(size).arg(contactName)
                            : qsTr("%1 · connecting…").arg(size)
        case stTransferring:
            return qsTr("%1 of %2").arg(account.transfers.formatSize(fileDone)).arg(size)
        case stUploading:
            return qsTr("Uploading · %1 of %2").arg(account.transfers.formatSize(fileDone)).arg(size)
        case stDone:
            return outgoing ? (isLink ? qsTr("%1 · link sent").arg(size) : qsTr("%1 · sent").arg(size))
                            : qsTr("%1 · saved in Downloads").arg(size)
        case stCancelled:
            return qsTr("%1 · cancelled").arg(size)
        case stDeclined:
            return outgoing ? qsTr("%1 · not accepted").arg(size) : qsTr("%1 · declined").arg(size)
        default:
            return qsTr("%1 · failed").arg(size)
        }
    }

    Column {
        id: content
        x: Theme.paddingMedium
        y: Theme.paddingMedium
        width: bubble.width - 2 * Theme.paddingMedium
        spacing: Theme.paddingSmall

        Row {
            width: parent.width
            spacing: Theme.paddingMedium

            Image {
                id: icon
                source: "image://theme/icon-m-attach?" + (bubble.outgoing ? Theme.highlightColor : Theme.primaryColor)
                anchors.verticalCenter: parent.verticalCenter
            }

            Column {
                width: parent.width - icon.width - parent.spacing
                Label {
                    width: parent.width
                    wrapMode: Text.WrapAnywhere
                    maximumLineCount: 3
                    elide: Text.ElideMiddle
                    color: bubble.outgoing ? Theme.highlightColor : Theme.primaryColor
                    text: bubble.fileName
                }
                Label {
                    width: parent.width
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: bubble.stateText()
                }
            }
        }

        // A picture that is here: show it.
        Image {
            id: preview
            readonly property bool isImage: /\.(jpe?g|png|gif|webp|bmp)$/i.test(bubble.fileName)
            visible: isImage && bubble.filePath.length > 0 && status === Image.Ready
                     && (bubble.outgoing || bubble.fileState === bubble.stDone)
            width: parent.width
            height: visible ? Math.min(width, implicitHeight * width / Math.max(1, implicitWidth)) : 0
            source: isImage && bubble.filePath.length > 0 && (bubble.outgoing || bubble.fileState === bubble.stDone)
                    ? "file://" + bubble.filePath.split("/").map(encodeURIComponent).join("/") : ""
            sourceSize.width: Math.ceil(width)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            clip: true
            MouseArea {
                anchors.fill: parent
                onClicked: pageStack.push(Qt.resolvedUrl("../pages/ImageViewerPage.qml"),
                                          { source: preview.source, filePath: bubble.filePath, link: bubble.fileUrl })
            }
        }

        // Progress
        Rectangle {
            visible: bubble.fileState === bubble.stTransferring || bubble.fileState === bubble.stUploading
            width: parent.width
            height: Theme.paddingSmall
            radius: height / 2
            color: Theme.rgba(Theme.secondaryColor, 0.3)
            Rectangle {
                width: parent.width * bubble.progress
                height: parent.height
                radius: parent.radius
                color: Theme.highlightColor
            }
        }

        Label {
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.errorColor
            text: {
                bubble.fileState
                return bubble.fileState === bubble.stFailed ? account.transfers.errorOf(bubble.transferId) : ""
            }
        }

        Flow {
            width: parent.width
            spacing: Theme.paddingMedium

            Button {
                visible: !bubble.outgoing && bubble.fileState === bubble.stOffered
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Accept")
                onClicked: {
                    if (!account.transfers.accept(bubble.transferId))
                        bubble.message(qsTr("This offer is no longer open."))
                }
            }
            Button {
                visible: !bubble.outgoing && bubble.fileState === bubble.stOffered
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Decline")
                onClicked: account.transfers.decline(bubble.transferId)
            }
            Button {
                visible: bubble.active && (bubble.outgoing || bubble.fileState !== bubble.stOffered)
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Cancel")
                onClicked: account.transfers.cancel(bubble.transferId)
            }
            Button {
                visible: !bubble.outgoing && bubble.fileState === bubble.stDone && bubble.filePath.length > 0
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Open")
                onClicked: Qt.openUrlExternally("file://" + bubble.filePath)
            }
            Button {
                visible: bubble.fileUrl.length > 0
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Copy link")
                onClicked: {
                    Clipboard.text = bubble.fileUrl
                    bubble.message(qsTr("Link copied"))
                }
            }
            Button {
                visible: bubble.outgoing && !bubble.isLink && !bubble.active && bubble.fileState !== bubble.stDone
                         && bubble.filePath.length > 0 && account.state === 2
                preferredWidth: Theme.buttonWidthSmall
                text: qsTr("Send as link")
                onClicked: {
                    if (account.transfers.retryAsLink(bubble.transferId, 72).length === 0) {
                        // After a restart the transfer is only in the history.
                        var id = account.transfers.sendLink(bubble.contact, bubble.filePath, 72)
                        if (id.length === 0)
                            bubble.message(account.transfers.lastError())
                    }
                }
            }
        }

        Label {
            anchors.right: parent.right
            font.pixelSize: Theme.fontSizeTiny
            color: Theme.secondaryColor
            text: Format.formatDate(bubble.timestamp, Formatter.TimeValue)
        }
    }
}
