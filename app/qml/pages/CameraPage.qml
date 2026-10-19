import QtQuick 2.6
import Sailfish.Silica 1.0
import QtMultimedia 5.6

// Takes a photo with the phone's camera, then shows it to send or retake.
// photoTaken(path) is emitted when you accept; the page pops itself.
Page {
    id: page
    allowedOrientations: Orientation.Portrait
    backNavigation: true

    property string savePath          // where to put the photo (no extension needed)
    property string acceptText: qsTr("Send")
    property string captured
    property bool front: false
    // Extra turn of the taken photo (degrees clockwise), chosen with "Turn".
    property int turn: 0
    signal photoTaken(string path)

    readonly property bool hasFront: QtMultimedia.availableCameras.length > 1
    // The camera is opened only while this page is on top (and closed for a
    // moment when switching cameras): opening it twice, or while the page is
    // still sliding in, makes the phone's camera library refuse it.
    property bool reloading: false
    property bool ready: false
    property string errorText
    readonly property bool wanted: ready && !reloading && page.status === PageStatus.Active
                                   && page.captured.length === 0 && Qt.application.active

    function reload() {
        errorText = ""
        reloading = true
        reloadTimer.restart()
    }

    function switchCamera() {
        front = !front
        reloading = true
        // Pick the camera while it is closed.
        var wantedPosition = front ? Camera.FrontFace : Camera.BackFace
        for (var i = 0; i < QtMultimedia.availableCameras.length; ++i) {
            var device = QtMultimedia.availableCameras[i]
            if (device.position === wantedPosition) {
                camera.deviceId = device.deviceId
                break
            }
        }
        reloadTimer.restart()
    }

    onStatusChanged: if (status === PageStatus.Active && !ready) startTimer.start()

    Timer {
        id: startTimer
        interval: 250
        onTriggered: page.ready = true
    }

    Timer {
        id: reloadTimer
        interval: 600
        onTriggered: page.reloading = false
    }

    Camera {
        id: camera
        captureMode: Camera.CaptureStillImage
        cameraState: page.wanted ? Camera.ActiveState : Camera.UnloadedState
        flash.mode: Camera.FlashOff
        // The page is portrait only. Sailfish counts the sensor's mounting
        // the other way round from this, hence 360 - orientation (the photo
        // is stored upright, or marked so).
        metaData.orientation: (360 - camera.orientation) % 360
        imageCapture {
            onImageSaved: page.captured = path
            onCaptureFailed: page.errorText = message
        }
        onError: {
            console.warn("camera error", errorCode, errorString, "availability", availability)
            page.errorText = errorString
        }
        onCameraStatusChanged: if (cameraStatus === Camera.ActiveStatus) page.errorText = ""
    }

    VideoOutput {
        id: viewfinder
        anchors.fill: parent
        visible: page.captured.length === 0
        source: camera
        fillMode: VideoOutput.PreserveAspectFit
        // The page does not rotate, so neither does the picture. (Following
        // the screen's orientation, as autoOrientation does, turned it
        // sideways on Sailfish OS.)
        orientation: 0
    }

    Image {
        anchors.centerIn: parent
        width: page.turn % 180 === 0 ? parent.width : parent.height
        height: page.turn % 180 === 0 ? parent.height : parent.width
        rotation: page.turn
        visible: page.captured.length > 0
        source: page.captured.length > 0 ? "file://" + page.captured : ""
        fillMode: Image.PreserveAspectFit
        autoTransform: true
        asynchronous: true
        cache: false
        sourceSize.width: 1600
        sourceSize.height: 1600
    }

    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        spacing: Theme.paddingLarge
        visible: page.captured.length === 0 && (page.errorText.length > 0 || QtMultimedia.availableCameras.length === 0)

        Label {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: "white"
            text: QtMultimedia.availableCameras.length === 0 ? qsTr("No camera found")
                  : qsTr("The camera cannot be used right now (%1).").arg(page.errorText)
        }
        Label {
            width: parent.width
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSizeSmall
            color: Theme.rgba("white", 0.7)
            visible: QtMultimedia.availableCameras.length > 0
            text: qsTr("Another app may be using it (the Camera app, or an Android app). Close it and try again, or choose a photo from the gallery instead.")
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: QtMultimedia.availableCameras.length > 0
            text: qsTr("Try again")
            onClicked: page.reload()
        }
    }

    // Shutter and switches.
    Item {
        visible: page.captured.length === 0
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge * 2
        }
        height: Theme.itemSizeLarge

        IconButton {
            anchors {
                left: parent.left
                leftMargin: Theme.horizontalPageMargin
                verticalCenter: parent.verticalCenter
            }
            icon.source: "image://theme/icon-m-back?white"
            onClicked: pageStack.pop()
        }

        Rectangle {
            anchors.centerIn: parent
            width: Theme.itemSizeLarge
            height: width
            radius: width / 2
            color: shutter.pressed ? Theme.highlightColor : "white"
            opacity: camera.imageCapture.ready ? 1 : 0.4
            border.width: Theme.paddingSmall
            border.color: Theme.rgba("black", 0.3)
            MouseArea {
                id: shutter
                anchors.fill: parent
                enabled: camera.imageCapture.ready
                onClicked: {
                    page.errorText = ""
                    camera.imageCapture.captureToLocation(page.savePath)
                }
            }
        }

        IconButton {
            visible: page.hasFront
            anchors {
                right: parent.right
                rightMargin: Theme.horizontalPageMargin
                verticalCenter: parent.verticalCenter
            }
            icon.source: "image://theme/icon-m-rotate?white"
            onClicked: page.switchCamera()
        }
    }

    // Use it or take another.
    Row {
        visible: page.captured.length > 0
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge * 2
        }
        spacing: Theme.paddingLarge

        Button {
            preferredWidth: Theme.buttonWidthExtraSmall !== undefined ? Theme.buttonWidthExtraSmall : Theme.buttonWidthSmall / 1.5
            text: qsTr("Retake")
            onClicked: {
                page.turn = 0
                page.captured = ""
            }
        }
        Button {
            preferredWidth: Theme.buttonWidthExtraSmall !== undefined ? Theme.buttonWidthExtraSmall : Theme.buttonWidthSmall / 1.5
            text: qsTr("Turn")
            onClicked: page.turn = (page.turn + 90) % 360
        }
        Button {
            preferredWidth: Theme.buttonWidthExtraSmall !== undefined ? Theme.buttonWidthExtraSmall : Theme.buttonWidthSmall / 1.5
            text: page.acceptText
            onClicked: {
                if (page.turn !== 0 && !account.media.rotateImage(page.captured, page.turn)) {
                    page.errorText = account.media.lastError()
                    return
                }
                var path = page.captured
                page.photoTaken(path)
                pageStack.pop()
            }
        }
    }
}
