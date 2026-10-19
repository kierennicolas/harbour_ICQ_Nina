import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import QtMultimedia 5.6
import harbour.icqnina 1.0
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
    property string picture
    // Messages in the outbox for this conversation.
    property int queued: 0
    // A file or picture picked in a picker, waiting for the picker to close.
    property var pickedFile: null
    property string pickedFor        // "file", "photo" or "sticker"
    // What is open below the messages: "", "attach", "emoji".
    property string panel: ""
    readonly property bool canSend: account.state === 2 && !page.blocked

    function refreshContact() {
        presence = account.contacts.presenceOf(uin)
        typing = account.contacts.isTyping(uin)
        name = account.displayName(uin)
        blocked = account.isBlocked(uin)
        picture = account.pictureOf(uin)
        queued = account.queuedCount(uin)
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

    function showInfo(text) {
        infoLabel.text = text
        infoTimer.restart()
    }

    function togglePanel(which) {
        panel = panel === which ? "" : which
        if (panel !== "")
            input.focus = false
    }

    function insertEmoji(text) {
        var pos = input.cursorPosition
        input.text = input.text.slice(0, pos) + text + input.text.slice(pos)
        input.cursorPosition = pos + text.length
    }

    // ---- files, photos, stickers ------------------------------------------

    function pickFile() {
        panel = ""
        pickedFor = "file"
        pageStack.push(filePicker)
    }

    function pickPhoto() {
        panel = ""
        pickedFor = "photo"
        pageStack.push(imagePicker)
    }

    function pickStickerPicture() {
        pickedFor = "sticker"
        pageStack.push(imagePicker)
    }

    function takePhoto() {
        panel = ""
        var cam = pageStack.push(Qt.resolvedUrl("CameraPage.qml"),
                                 { savePath: account.media.newFilePath("camera", "jpg") })
        cam.photoTaken.connect(function(path) { page.sendCameraPhoto(path) })
    }

    function sendCameraPhoto(path) {
        if (!account.media.sendPhoto(uin, path))
            showInfo(account.media.lastError())
        // The phone keeps no copy: the smaller one sent is in the conversation.
        account.media.discard(path)
    }

    function sendSticker(stickerId) {
        if (!account.media.sendSticker(uin, stickerId))
            showInfo(account.media.lastError())
    }

    function removeSticker(stickerId) {
        stickerRemorse.execute(qsTr("Removing sticker"), function() { account.media.stickers.remove(stickerId) })
    }

    function keepSticker(path, stickerId) {
        var id = account.media.stickers.add(path, stickerId)
        showInfo(id.length > 0 ? qsTr("Added to your stickers") : account.media.stickers.lastError())
    }

    function handlePicked() {
        var f = pickedFile
        var what = pickedFor
        pickedFile = null
        if (!f || !f.filePath)
            return
        if (what === "sticker") {
            var id = account.media.stickers.add(f.filePath)
            if (id.length === 0)
                showInfo(account.media.stickers.lastError())
            else
                panel = "emoji"
        } else if (what === "photo") {
            var d = pageStack.push(Qt.resolvedUrl("PhotoSendDialog.qml"), { filePath: f.filePath, name: name })
            d.accepted.connect(function() { page.sendPhoto(f.filePath) })
        } else {
            var s = pageStack.push(Qt.resolvedUrl("SendFileDialog.qml"), {
                uin: uin, name: name, filePath: f.filePath, fileName: f.fileName || f.filePath.split("/").pop(),
                fileSize: f.fileSize || 0, contactOnline: presence !== Presence.Offline })
            s.failed.connect(function(message) { page.showInfo(message) })
        }
    }

    function sendPhoto(path) {
        if (!account.media.sendPhoto(uin, path))
            showInfo(account.media.lastError())
    }

    // ---- voice messages ---------------------------------------------------

    function startVoice() {
        panel = ""
        input.focus = false
        player.stop()
        if (!recorder.start(account.media.newFilePath("voice")))
            showInfo(recorder.error)
    }

    // ---- message actions --------------------------------------------------

    function copyText(text) {
        Clipboard.text = text
        showInfo(qsTr("Copied"))
    }

    Component.onCompleted: refreshContact()

    onStatusChanged: {
        if (status === PageStatus.Active) {
            account.activeChat = uin
            app.clearNotification(uin)
            messages.positionViewAtEnd()
            if (pickedFile)
                handlePicked()
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

    Component {
        id: imagePicker
        ImagePickerPage {
            title: page.pickedFor === "sticker" ? qsTr("Make a sticker") : qsTr("Send photo")
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties && selectedContentProperties.filePath)
                    page.pickedFile = { filePath: selectedContentProperties.filePath }
            }
        }
    }

    RemorsePopup { id: stickerRemorse }

    Audio {
        id: player
        autoLoad: false
        onError: page.showInfo(errorString)
    }

    VoiceRecorder {
        id: recorder
        onFinished: {
            if (!account.media.sendVoice(page.uin, path, duration))
                page.showInfo(account.media.lastError())
        }
        onFailed: page.showInfo(message)
    }

    Connections {
        target: account.contacts
        onPresenceChanged: page.refreshContact()
        onDataChanged: page.refreshContact()
        onModelReset: page.refreshContact()
    }

    Connections {
        target: account
        onQueueChanged: if (uin === page.uin) page.queued = account.queuedCount(page.uin)
        onStateChanged: page.refreshContact()
        onPrivacyChanged: page.refreshContact()
        onPictureChanged: page.refreshContact()
    }

    Connections {
        target: account.media
        onFailed: if (uin === page.uin) page.showInfo(message)
    }

    Connections {
        target: Qt.application
        onActiveChanged: if (!Qt.application.active) player.pause()
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
            bottom: bottomArea.top
        }
        clip: true
        model: page.chat

        PullDownMenu {
            MenuItem {
                text: qsTr("Contact details")
                onClicked: pageStack.push(Qt.resolvedUrl("ContactInfoPage.qml"), { uin: page.uin })
            }
            MenuItem {
                visible: page.canSend
                text: qsTr("Send file")
                onClicked: page.pickFile()
            }
        }

        header: Item {
            width: messages.width
            height: pageHeader.height

            PageHeader {
                id: pageHeader
                width: parent.width
                rightMargin: Theme.horizontalPageMargin + headerAvatar.width + Theme.paddingMedium
                title: page.name
                description: page.typing ? qsTr("typing…")
                             : Presence.name(page.presence) + (page.name !== page.uin ? " · " + page.uin : "")
            }
            Avatar {
                id: headerAvatar
                anchors {
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: pageHeader.verticalCenter
                }
                size: Theme.iconSizeMedium
                source: page.picture
                name: page.name
                uin: page.uin
                presence: page.presence
            }
            MouseArea {
                anchors.fill: headerAvatar
                onClicked: pageStack.push(Qt.resolvedUrl("ContactInfoPage.qml"), { uin: page.uin })
            }
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

        delegate: ListItem {
            id: row
            width: messages.width
            contentHeight: (model.kind === 1 ? fileBubble.height : model.kind === 2 ? mediaBubble.height : bubble.height)
                           + Theme.paddingSmall
            // Text messages and files open a menu; photos and voice messages
            // use taps themselves, so a long press opens it.
            onClicked: if (model.kind === 0) openMenu()
            menu: Component {
                ContextMenu {
                    MenuItem {
                        visible: model.outgoing && model.queued && account.state === 2
                        text: qsTr("Send now")
                        onClicked: account.sendQueuedNow(page.uin, model.messageId)
                    }
                    MenuItem {
                        visible: model.outgoing && model.queued
                        text: qsTr("Delete (do not send)")
                        onClicked: account.deleteQueued(page.uin, model.messageId)
                    }
                    MenuItem {
                        visible: model.kind === 0
                        text: qsTr("Copy")
                        onClicked: page.copyText(model.text)
                    }
                    MenuItem {
                        visible: model.kind === 2 && (model.fileUrl || "").length > 0
                        text: qsTr("Copy link")
                        onClicked: page.copyText(model.fileUrl.split("#")[0])
                    }
                    MenuItem {
                        visible: model.kind === 2 && model.mediaType === 1 && model.fileState === 3
                        text: qsTr("Save to Pictures")
                        onClicked: {
                            var saved = account.media.saveToPictures(model.filePath)
                            page.showInfo(saved.length > 0 ? qsTr("Saved in Pictures/ICQ Nina") : account.media.lastError())
                        }
                    }
                    MenuItem {
                        visible: model.kind === 2 && model.mediaType === 3 && !model.outgoing && model.fileState === 3
                                 && !account.media.stickers.contains(model.mediaInfo)
                        text: qsTr("Add to my stickers")
                        onClicked: page.keepSticker(model.filePath, model.mediaInfo)
                    }
                }
            }

            Rectangle {
                id: bubble
                visible: model.kind === 0
                readonly property real maxWidth: row.width * 0.8
                readonly property int bigEmoji: model.kind === 0 ? emoji.emojiOnly(model.text) : 0
                anchors {
                    right: model.outgoing ? parent.right : undefined
                    left: model.outgoing ? undefined : parent.left
                    rightMargin: Theme.horizontalPageMargin
                    leftMargin: Theme.horizontalPageMargin
                }
                width: Math.min(maxWidth, Math.max(textLabel.implicitWidth, timeLabel.implicitWidth) + 2 * Theme.paddingMedium)
                height: textLabel.height + timeLabel.height + 2 * Theme.paddingSmall
                radius: Theme.paddingMedium
                color: bigEmoji > 0 ? "transparent"
                     : model.outgoing ? Theme.rgba(Theme.highlightBackgroundColor, row.highlighted ? 0.55 : 0.35)
                     : Theme.rgba(Theme.secondaryColor, row.highlighted ? 0.3 : 0.15)

                Label {
                    id: textLabel
                    x: Theme.paddingMedium
                    y: Theme.paddingSmall
                    width: bubble.maxWidth - 2 * Theme.paddingMedium
                    text: model.kind === 0
                          ? emoji.richText(model.text, bubble.bigEmoji > 0 ? Theme.iconSizeLarge : Math.round(font.pixelSize * 1.15))
                          : ""
                    wrapMode: Text.Wrap
                    textFormat: Text.StyledText
                    color: model.outgoing ? Theme.highlightColor : Theme.primaryColor
                    linkColor: model.outgoing ? Theme.primaryColor : Theme.highlightColor
                    onLinkActivated: Qt.openUrlExternally(link)
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
                          + (!model.outgoing ? "" : model.queued ? " · " + qsTr("waiting to be sent")
                             : model.delivered ? " ✓" : " …")
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

            MediaBubble {
                id: mediaBubble
                visible: model.kind === 2
                anchors {
                    right: model.outgoing ? parent.right : undefined
                    left: model.outgoing ? undefined : parent.left
                    rightMargin: Theme.horizontalPageMargin
                    leftMargin: Theme.horizontalPageMargin
                }
                maxWidth: row.width * 0.8
                outgoing: model.outgoing
                mediaType: model.mediaType || 0
                mediaInfo: model.mediaInfo || ""
                fileState: model.fileState || 0
                fileDone: model.fileDone || 0
                fileSize: model.fileSize || 0
                filePath: model.filePath || ""
                fileUrl: model.fileUrl || ""
                messageId: model.messageId
                timestamp: model.timestamp
                delivered: model.delivered
                queued: model.queued
                contact: page.uin
                player: player
                onMessage: page.showInfo(text)
                onHoldRequested: row.openMenu()
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
        id: bottomArea
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

        // The outbox.
        Label {
            visible: page.queued > 0 && !page.blocked
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: account.state !== 2
                  ? qsTr("%n message(s) waiting: they are sent when you are online.", "", page.queued)
                  : qsTr("%n message(s) waiting: they are sent when %1 comes online.", "", page.queued).arg(page.name)
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
            visible: !page.blocked && page.queued === 0 && account.isContact(page.uin)
                     && page.presence === Presence.Offline && account.state === 2
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.secondaryHighlightColor
            font.pixelSize: Theme.fontSizeExtraSmall
            text: qsTr("%1 is offline. Messages are kept (by the server, or here until they come online) and arrive when they are back.").arg(page.name)
        }

        // Send a photo, take one, or send a file.
        Row {
            id: attachRow
            visible: page.panel === "attach"
            width: parent.width
            height: Theme.itemSizeLarge

            Repeater {
                model: [
                    { icon: "image://theme/icon-m-image", text: qsTr("Photo"), action: "photo" },
                    { icon: "image://theme/icon-m-camera", text: qsTr("Camera"), action: "camera" },
                    { icon: "image://theme/icon-m-document", text: qsTr("File"), action: "file" }
                ]
                BackgroundItem {
                    width: attachRow.width / 3
                    height: attachRow.height
                    enabled: page.canSend
                    onClicked: {
                        if (modelData.action === "photo")
                            page.pickPhoto()
                        else if (modelData.action === "camera")
                            page.takePhoto()
                        else
                            page.pickFile()
                    }
                    Column {
                        anchors.centerIn: parent
                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            source: modelData.icon + "?" + (parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor)
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: parent.parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.text
                        }
                    }
                }
            }
        }

        EmojiPanel {
            id: emojiPanel
            visible: page.panel === "emoji"
            width: parent.width
            height: visible ? Math.round((page.isPortrait ? page.height * 0.38 : page.height * 0.5)) : 0
            onEmojiPicked: page.insertEmoji(text)
            onStickerPicked: page.sendSticker(stickerId)
            onAddStickerRequested: page.pickStickerPicture()
            onRemoveStickerRequested: page.removeSticker(stickerId)
        }

        // Recording a voice message.
        Item {
            visible: recorder.recording || recorder.finishing
            width: parent.width
            height: Theme.itemSizeMedium

            IconButton {
                id: cancelVoice
                anchors {
                    left: parent.left
                    leftMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                icon.source: "image://theme/icon-m-delete"
                onClicked: recorder.cancel()
            }
            Rectangle {
                id: recDot
                anchors {
                    left: cancelVoice.right
                    leftMargin: Theme.paddingMedium
                    verticalCenter: parent.verticalCenter
                }
                width: Theme.paddingLarge
                height: width
                radius: width / 2
                color: "#e8423f"
                SequentialAnimation on opacity {
                    running: recorder.recording
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.2; duration: 600 }
                    NumberAnimation { to: 1; duration: 600 }
                }
            }
            Label {
                anchors {
                    left: recDot.right
                    leftMargin: Theme.paddingMedium
                    right: sendVoice.left
                    verticalCenter: parent.verticalCenter
                }
                truncationMode: TruncationMode.Fade
                color: Theme.highlightColor
                text: {
                    var s = Math.floor(recorder.duration / 1000)
                    var left = Math.floor((recorder.maxDuration - recorder.duration) / 1000)
                    return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + (s % 60) + "  "
                           + (recorder.finishing ? qsTr("Finishing…")
                              : left < 30 ? qsTr("%n second(s) left", "", left) : qsTr("Recording"))
                }
            }
            IconButton {
                id: sendVoice
                anchors {
                    right: parent.right
                    rightMargin: Theme.paddingSmall
                    verticalCenter: parent.verticalCenter
                }
                enabled: recorder.recording
                icon.source: "image://theme/icon-m-send"
                onClicked: recorder.stop()
            }
        }

        Item {
            visible: !recorder.recording && !recorder.finishing
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
                icon.source: page.panel === "attach" ? "image://theme/icon-m-close" : "image://theme/icon-m-attach"
                enabled: page.canSend
                onClicked: page.togglePanel("attach")
            }

            TextArea {
                id: input
                anchors {
                    left: attachButton.right
                    right: emojiButton.left
                    bottom: parent.bottom
                }
                placeholderText: account.state === 2 ? qsTr("Message") : qsTr("Message (sent when you are online)")
                enabled: account.hasAccount
                onActiveFocusChanged: if (activeFocus) page.panel = ""
                onTextChanged: {
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
                id: emojiButton
                anchors {
                    right: sendButton.left
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                }
                enabled: account.state === 2
                icon.source: page.panel === "emoji" ? "image://theme/icon-m-keyboard" : ""
                onClicked: {
                    if (page.panel === "emoji") {
                        page.panel = ""
                        input.forceActiveFocus()
                    } else {
                        page.togglePanel("emoji")
                    }
                }
                Image {
                    visible: page.panel !== "emoji"
                    anchors.centerIn: parent
                    width: Theme.iconSizeSmallPlus
                    height: width
                    sourceSize.width: width
                    opacity: parent.enabled ? (parent.down ? 0.6 : 1) : 0.4
                    source: emoji.urlOf("🙂")
                }
            }

            // Send when there is text, else record a voice message.
            IconButton {
                id: sendButton
                readonly property bool hasText: input.text.trim().length > 0
                anchors {
                    right: parent.right
                    rightMargin: Theme.paddingSmall
                    bottom: parent.bottom
                    bottomMargin: Theme.paddingMedium
                }
                icon.source: hasText ? "image://theme/icon-m-send" : "image://theme/icon-m-mic"
                // Text can be written offline (it waits in the outbox).
                enabled: hasText ? account.hasAccount && !page.blocked : page.canSend
                onClicked: hasText ? page.send() : page.startVoice()
            }
        }
    }
}
