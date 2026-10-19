import QtQuick 2.6
import Sailfish.Silica 1.0
import QtMultimedia 5.6

// A photo, voice message or sticker in a conversation (see MediaLink in the
// core): shows it when it is here, its progress while it travels, and lets
// you load it or try again.
Item {
    id: bubble

    property bool outgoing
    property int mediaType          // 1 photo, 2 voice, 3 sticker
    property string mediaInfo
    property int fileState          // TransferManager::State
    property real fileDone
    property real fileSize
    property string filePath
    property string fileUrl
    property var messageId
    property var timestamp
    property bool delivered
    property bool queued
    property string contact
    property real maxWidth: 400
    // The conversation's audio player (one for all voice messages).
    property var player

    signal message(string text)
    // Long press on a photo or sticker: the conversation opens its menu.
    signal holdRequested()

    readonly property int stOffered: 0
    readonly property int stTransferring: 2
    readonly property int stDone: 3
    readonly property int stFailed: 4
    readonly property int stUploading: 7
    // The file is on the phone (what you send always is).
    readonly property bool here: filePath.length > 0 && (outgoing || fileState === stDone)
    readonly property bool busy: fileState === stTransferring || fileState === stUploading
    readonly property real progress: fileSize > 0 ? Math.min(1, fileDone / fileSize) : 0
    readonly property string localUrl: filePath.length > 0 ? "file://" + filePath.split("/").map(encodeURIComponent).join("/") : ""
    readonly property string timeText: Format.formatDate(timestamp, Formatter.TimeValue)
                                       + (outgoing ? (fileState === stFailed ? " ⚠" : queued ? " · " + qsTr("waiting")
                                                      : delivered ? " ✓" : " …") : "")

    width: mediaType === 2 ? voice.width : mediaType === 3 ? stickerBox.width : photoBox.width
    height: mediaType === 2 ? voice.height : mediaType === 3 ? stickerBox.height : photoBox.height

    function act() {
        if (busy) {
            account.media.cancel(contact, messageId)
        } else if (fileState === stFailed) {
            account.media.retry(contact, messageId)
        } else if (!outgoing && fileState === stOffered) {
            account.media.load(contact, messageId)
        } else if (here && mediaType === 1) {
            pageStack.push(Qt.resolvedUrl("../pages/ImageViewerPage.qml"), { source: localUrl, filePath: filePath, link: fileUrl })
        } else if (here && mediaType === 2) {
            togglePlay()
        }
    }

    function togglePlay() {
        if (!player)
            return
        if (player.source.toString() === localUrl) {
            if (player.playbackState === Audio.PlayingState)
                player.pause()
            else
                player.play()
        } else {
            player.stop()
            player.source = localUrl
            player.play()
        }
    }

    // What to say instead of the picture while it is not here.
    function placeholderText() {
        if (fileState === stUploading)
            return qsTr("Sending…")
        if (fileState === stTransferring)
            return qsTr("Loading…")
        if (fileState === stFailed)
            return outgoing ? qsTr("Not sent. Tap to try again.") : qsTr("Could not load. Tap to try again.")
        if (!outgoing && fileState === stOffered)
            return mediaType === 3 ? qsTr("Sticker. Tap to load.")
                 : mediaType === 2 ? qsTr("Tap to load.") : qsTr("Photo. Tap to load.")
        return qsTr("The file is gone.")
    }

    // ---- photo ---------------------------------------------------------
    Item {
        id: photoBox
        visible: bubble.mediaType === 1
        readonly property var dims: {
            var m = /^(\d+)x(\d+)$/.exec(bubble.mediaInfo)
            return m ? [parseInt(m[1]), parseInt(m[2])] : [4, 3]
        }
        width: Math.min(bubble.maxWidth, Theme.itemSizeHuge * 2.4)
        height: Math.max(width * 0.45, Math.min(width * 1.33, width * dims[1] / Math.max(1, dims[0])))

        Rectangle {
            anchors.fill: parent
            radius: Theme.paddingMedium
            color: bubble.outgoing ? Theme.rgba(Theme.highlightBackgroundColor, 0.35) : Theme.rgba(Theme.secondaryColor, 0.15)
        }

        Image {
            id: photo
            anchors.fill: parent
            visible: bubble.mediaType === 1 && bubble.here && status === Image.Ready
            source: bubble.mediaType === 1 && bubble.here ? bubble.localUrl : ""
            sourceSize.width: Math.ceil(width)
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            smooth: true
            clip: true
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.paddingMedium
            visible: photo.visible && bubble.fileState === bubble.stFailed
            color: Theme.rgba("black", 0.55)
        }

        Column {
            visible: !photo.visible || bubble.fileState === bubble.stFailed
            anchors.centerIn: parent
            width: parent.width - 2 * Theme.paddingLarge
            spacing: Theme.paddingSmall

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: !bubble.busy
                source: (bubble.fileState === bubble.stFailed ? "image://theme/icon-m-refresh"
                         : !bubble.outgoing && bubble.fileState === bubble.stOffered ? "image://theme/icon-m-cloud-download"
                         : "image://theme/icon-m-image") + "?" + Theme.primaryColor
            }
            Label {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: photo.visible ? "white" : Theme.secondaryColor
                text: bubble.placeholderText()
            }
        }

        ProgressCircle {
            anchors.centerIn: parent
            visible: bubble.busy
            width: Theme.itemSizeMedium
            height: width
            value: bubble.progress
            progressColor: Theme.highlightColor
            backgroundColor: Theme.rgba(Theme.highlightDimmerColor, 0.6)
        }

        Rectangle {
            anchors {
                right: parent.right
                bottom: parent.bottom
                margins: Theme.paddingSmall
            }
            width: photoTime.implicitWidth + Theme.paddingMedium
            height: photoTime.implicitHeight + Theme.paddingSmall / 2
            radius: height / 2
            color: Theme.rgba("black", 0.45)
            Label {
                id: photoTime
                anchors.centerIn: parent
                font.pixelSize: Theme.fontSizeTiny
                color: "white"
                text: bubble.timeText
            }
        }
    }

    // ---- sticker -------------------------------------------------------
    Item {
        id: stickerBox
        visible: bubble.mediaType === 3
        width: Math.min(bubble.maxWidth * 0.7, Theme.itemSizeHuge * 1.4)
        height: width + stickerTime.height

        Image {
            id: sticker
            width: parent.width
            height: width
            visible: bubble.mediaType === 3 && bubble.here && status === Image.Ready
            source: bubble.mediaType === 3 && bubble.here ? bubble.localUrl : ""
            sourceSize.width: 256
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
        }

        Rectangle {
            visible: !sticker.visible
            width: parent.width
            height: width
            radius: Theme.paddingLarge
            color: Theme.rgba(Theme.secondaryColor, 0.1)
            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.paddingMedium
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: bubble.placeholderText()
            }
            BusyIndicator {
                anchors.centerIn: parent
                running: bubble.busy
                visible: running
                size: BusyIndicatorSize.Small
            }
        }

        Label {
            id: stickerTime
            anchors {
                top: sticker.bottom
                right: bubble.outgoing ? parent.right : undefined
                left: bubble.outgoing ? undefined : parent.left
            }
            font.pixelSize: Theme.fontSizeTiny
            color: Theme.secondaryColor
            text: bubble.timeText
        }
    }

    // ---- voice message -------------------------------------------------
    Rectangle {
        id: voice
        visible: bubble.mediaType === 2
        width: Math.min(bubble.maxWidth, Theme.itemSizeHuge * 2.6)
        height: Theme.itemSizeMedium + voiceTime.height
        radius: Theme.paddingMedium
        color: bubble.outgoing ? Theme.rgba(Theme.highlightBackgroundColor, 0.35) : Theme.rgba(Theme.secondaryColor, 0.15)

        readonly property bool current: !!bubble.player && bubble.localUrl.length > 0
                                        && bubble.player.source.toString() === bubble.localUrl
        readonly property bool playing: current && bubble.player.playbackState === Audio.PlayingState
        readonly property real total: {
            var ms = parseInt(bubble.mediaInfo)
            if (current && bubble.player.duration > 0)
                return bubble.player.duration
            return ms > 0 ? ms : 0
        }
        readonly property real position: current ? bubble.player.position : 0

        function clock(ms) {
            var s = Math.round(ms / 1000)
            return Math.floor(s / 60) + ":" + (s % 60 < 10 ? "0" : "") + (s % 60)
        }

        IconButton {
            id: playButton
            anchors {
                left: parent.left
                leftMargin: Theme.paddingSmall
                top: parent.top
                topMargin: (Theme.itemSizeMedium - height) / 2
            }
            icon.source: bubble.busy ? "image://theme/icon-m-clear"
                         : bubble.fileState === bubble.stFailed ? "image://theme/icon-m-refresh"
                         : !bubble.here ? "image://theme/icon-m-cloud-download"
                         : voice.playing ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
            onClicked: bubble.act()

            ProgressCircle {
                anchors.centerIn: parent
                visible: bubble.busy
                width: parent.width * 0.9
                height: width
                value: bubble.progress
                progressColor: Theme.highlightColor
                backgroundColor: "transparent"
            }
        }

        // Position; tap or drag to move in the message.
        Item {
            id: track
            anchors {
                left: playButton.right
                right: parent.right
                rightMargin: Theme.paddingLarge
                verticalCenter: playButton.verticalCenter
            }
            height: Theme.itemSizeExtraSmall

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: Math.max(2, Theme.paddingSmall / 2)
                radius: height / 2
                color: Theme.rgba(Theme.secondaryColor, 0.4)
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: voice.total > 0 ? parent.width * Math.min(1, voice.position / voice.total) : 0
                height: Math.max(2, Theme.paddingSmall / 2)
                radius: height / 2
                color: Theme.highlightColor
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                x: (voice.total > 0 ? parent.width * Math.min(1, voice.position / voice.total) : 0) - width / 2
                width: Theme.paddingLarge
                height: width
                radius: width / 2
                visible: voice.current
                color: Theme.highlightColor
            }
            MouseArea {
                anchors.fill: parent
                enabled: voice.current && voice.total > 0
                function seekTo(x) {
                    bubble.player.seek(Math.max(0, Math.min(1, x / width)) * voice.total)
                }
                onPressed: seekTo(mouse.x)
                onPositionChanged: seekTo(mouse.x)
            }
        }

        Label {
            id: voiceTime
            anchors {
                left: track.left
                right: parent.right
                rightMargin: Theme.paddingMedium
                top: playButton.bottom
                topMargin: -Theme.paddingSmall
            }
            font.pixelSize: Theme.fontSizeTiny
            color: Theme.secondaryColor
            text: (voice.current ? voice.clock(voice.position) + " / " : "")
                  + (voice.total > 0 ? voice.clock(voice.total) : qsTr("Voice message"))
                  + (bubble.busy || bubble.fileState === bubble.stFailed || !bubble.here
                     ? " · " + bubble.placeholderText() : "")
                  + "   " + bubble.timeText
            truncationMode: TruncationMode.Fade
        }
    }

    MouseArea {
        // Photos and stickers: the whole thing is the button.
        visible: bubble.mediaType !== 2
        anchors.fill: parent
        onClicked: bubble.act()
        onPressAndHold: bubble.holdRequested()
    }
}
