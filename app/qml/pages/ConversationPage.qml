import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../components"
import "../components/Presence.js" as Presence

Page {
    id: page
    objectName: "conversation"
    allowedOrientations: Orientation.All

    property string uin
    property var chat: account.chat(uin)
    property bool typingSent: false
    // ContactModel changes do not notify per uin, so poll the bits we show.
    property int presence: Presence.Offline
    property bool typing: false
    property string name: uin
    property bool blocked: false
    // A file picked in the picker, waiting for the picker to close.
    property var pickedFile: null

    function refreshContact() {
        presence = account.contacts.presenceOf(uin)
        typing = account.contacts.isTyping(uin)
        name = account.displayName(uin)
        blocked = account.isBlocked(uin)
    }

    function send() {
        var text = input.text
        if (text.trim().length === 0)
            return
        if (account.sendMessage(uin, text)) {
            input.text = ""
            stopTyping()
        }
    }

    function stopTyping() {
        typingTimer.stop()
        if (typingSent) {
            typingSent = false
            account.setTyping(uin, false)
        }
    }

    function pickFile() {
        pageStack.push(filePicker)
    }

    function showInfo(text) {
        infoLabel.text = text
        infoTimer.restart()
    }

    function offerPickedFile() {
        var f = pickedFile
        pickedFile = null
        if (!f || !f.filePath)
            return
        var d = pageStack.push(Qt.resolvedUrl("SendFileDialog.qml"), {
            uin: uin, name: name, filePath: f.filePath, fileName: f.fileName || f.filePath.split("/").pop(),
            fileSize: f.fileSize || 0, contactOnline: presence !== Presence.Offline })
        d.failed.connect(function(message) { page.showInfo(message) })
    }

    Component.onCompleted: refreshContact()

    onStatusChanged: {
        if (status === PageStatus.Active) {
            account.activeChat = uin
            app.clearNotification(uin)
            messages.positionViewAtEnd()
            if (pickedFile)
                offerPickedFile()
        } else if (status === PageStatus.Deactivating && account.activeChat === uin) {
            account.activeChat = ""
            stopTyping()
        }
    }

    Component {
        id: filePicker
        FilePickerPage {
            title: qsTr("Send file")
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties && selectedContentProperties.filePath)
                    page.pickedFile = {
                        filePath: selectedContentProperties.filePath,
                        fileName: selectedContentProperties.fileName,
                        fileSize: selectedContentProperties.fileSize
                    }
            }
        }
    }

    Connections {
        target: account.contacts
        onPresenceChanged: page.refreshContact()
        onDataChanged: page.refreshContact()
        onModelReset: page.refreshContact()
    }

    Connections {
        target: account
        onMessageFailed: if (uin === page.uin) failedLabel.visible = true
        onPrivacyChanged: page.refreshContact()
    }

    Timer {
        id: typingTimer
        interval: 5000
        onTriggered: page.stopTyping()
    }

    Timer {
        id: infoTimer
        interval: 4000
        onTriggered: infoLabel.text = ""
    }

    SilicaListView {
        id: messages
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            bottom: inputRow.top
        }
        clip: true
        model: page.chat

        PullDownMenu {
            MenuItem {
                text: qsTr("Contact details")
                onClicked: pageStack.push(Qt.resolvedUrl("ContactInfoPage.qml"), { uin: page.uin })
            }
            MenuItem {
                visible: account.state === 2 && !page.blocked
                text: qsTr("Send file")
                onClicked: page.pickFile()
            }
        }

        header: PageHeader {
            title: page.name
            description: page.typing ? qsTr("typing…")
                         : Presence.name(page.presence) + (page.name !== page.uin ? " · " + page.uin : "")
        }

        section.property: "day"
        section.delegate: Label {
            width: messages.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            topPadding: Theme.paddingMedium
            bottomPadding: Theme.paddingSmall
            // noon, so a time zone shift cannot move the date
            text: Format.formatDate(new Date(section + "T12:00:00"), Formatter.DateMedium)
        }

        delegate: Item {
            id: row
            width: messages.width
            height: (model.kind === 1 ? fileBubble.height : bubble.height) + Theme.paddingSmall

            Rectangle {
                id: bubble
                visible: model.kind !== 1
                readonly property real maxWidth: row.width * 0.8
                anchors {
                    right: model.outgoing ? parent.right : undefined
                    left: model.outgoing ? undefined : parent.left
                    rightMargin: Theme.horizontalPageMargin
                    leftMargin: Theme.horizontalPageMargin
                }
                width: Math.min(maxWidth, Math.max(textLabel.implicitWidth, timeLabel.implicitWidth) + 2 * Theme.paddingMedium)
                height: textLabel.height + timeLabel.height + 2 * Theme.paddingSmall
                radius: Theme.paddingMedium
                color: model.outgoing ? Theme.rgba(Theme.highlightBackgroundColor, 0.35)
                                      : Theme.rgba(Theme.secondaryColor, 0.15)

                Label {
                    id: textLabel
                    x: Theme.paddingMedium
                    y: Theme.paddingSmall
                    width: bubble.maxWidth - 2 * Theme.paddingMedium
                    text: model.text
                    wrapMode: Text.Wrap
                    textFormat: Text.PlainText
                    color: model.outgoing ? Theme.highlightColor : Theme.primaryColor
                    // Shrink-wrap short messages.
                    Component.onCompleted: if (implicitWidth < width) width = implicitWidth
                }
                Label {
                    id: timeLabel
                    anchors {
                        top: textLabel.bottom
                        right: parent.right
                        rightMargin: Theme.paddingMedium
                    }
                    font.pixelSize: Theme.fontSizeTiny
                    color: Theme.secondaryColor
                    text: Format.formatDate(model.timestamp, Formatter.TimeValue)
                          + (model.outgoing ? (model.delivered ? " ✓" : " …") : "")
                }
            }

            FileBubble {
                id: fileBubble
                visible: model.kind === 1
                anchors {
                    right: model.outgoing ? parent.right : undefined
                    left: model.outgoing ? undefined : parent.left
                    rightMargin: Theme.horizontalPageMargin
                    leftMargin: Theme.horizontalPageMargin
                }
                maxWidth: row.width * 0.85
                outgoing: model.outgoing
                fileName: model.fileName || ""
                fileSize: model.fileSize || 0
                fileState: model.fileState || 0
                fileDone: model.fileDone || 0
                filePath: model.filePath || ""
                fileUrl: model.fileUrl || ""
                transferId: model.transferId || ""
                timestamp: model.timestamp
                contact: page.uin
                contactName: page.name
                onMessage: page.showInfo(text)
            }
        }

        onCountChanged: positionViewAtEnd()

        ViewPlaceholder {
            enabled: messages.count === 0
            text: qsTr("No messages yet")
            hintText: account.state === 2 ? qsTr("Say hello to %1").arg(page.name)
                                          : qsTr("Go online to send messages")
        }

        VerticalScrollDecorator {}
    }

    Column {
        id: inputRow
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }

        Label {
            id: infoLabel
            visible: text.length > 0
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
        }

        Label {
            id: failedLabel
            visible: false
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.errorColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: qsTr("The last message could not be delivered.")
        }

        Label {
            visible: page.blocked
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: qsTr("You blocked %1. Unblock them from their contact details.").arg(page.name)
        }

        Label {
            visible: !page.blocked && account.contacts.contains(page.uin) && page.presence === Presence.Offline && account.state === 2
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: qsTr("%1 is offline; the message will be delivered if the server keeps offline messages.").arg(page.name)
        }

        Item {
            width: parent.width
            height: Math.max(input.height, sendButton.height)

            IconButton {
                id: attachButton
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingSmall
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                }
                icon.source: "image://theme/icon-m-attach"
                enabled: account.state === 2 && !page.blocked
                onClicked: page.pickFile()
            }

            TextArea {
                id: input
                anchors {
                    left: attachButton.right
                    right: sendButton.left
                    bottom: parent.bottom
                }
                placeholderText: account.state === 2 ? qsTr("Message") : qsTr("Not connected")
                enabled: account.state === 2
                onTextChanged: {
                    failedLabel.visible = false
                    if (text.length === 0) {
                        page.stopTyping()
                        return
                    }
                    if (!page.typingSent) {
                        page.typingSent = true
                        account.setTyping(page.uin, true)
                    }
                    typingTimer.restart()
                }
            }

            IconButton {
                id: sendButton
                anchors {
                    right: parent.right
                    rightMargin: Theme.paddingSmall
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                }
                icon.source: "image://theme/icon-m-send"
                enabled: account.state === 2 && input.text.trim().length > 0
                onClicked: page.send()
            }
        }
    }
}
