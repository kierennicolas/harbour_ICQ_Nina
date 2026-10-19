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
    signal photoTaken(string path)

    readonly property bool hasFront: QtMultimedia.availableCameras.length > 1

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    Camera {
        id: camera
        deviceId: page.front && page.hasFront ? QtMultimedia.availableCameras[1].deviceId
                                              : QtMultimedia.availableCameras.length > 0 ? QtMultimedia.availableCameras[0].deviceId : ""
        captureMode: Camera.CaptureStillImage
        cameraState: page.status === PageStatus.Active && page.captured.length === 0 && Qt.application.active
                     ? Camera.ActiveState : Camera.LoadedState
        flash.mode: Camera.FlashOff
        focus.focusMode: Camera.FocusContinuous
        imageCapture {
            onImageSaved: page.captured = path
            onCaptureFailed: errorLabel.text = message
        }
        onError: errorLabel.text = errorString
    }

    VideoOutput {
        id: viewfinder
        anchors.fill: parent
        visible: page.captured.length === 0
        source: camera
        fillMode: VideoOutput.PreserveAspectFit
        autoOrientation: true
    }

    Image {
        anchors.fill: parent
        visible: page.captured.length > 0
        source: page.captured.length > 0 ? "file://" + page.captured : ""
        fillMode: Image.PreserveAspectFit
        autoTransform: true
        asynchronous: true
        sourceSize.width: 1600
        sourceSize.height: 1600
    }

    Label {
        id: errorLabel
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.horizontalPageMargin
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignHCenter
        visible: text.length > 0
        color: "white"
        text: QtMultimedia.availableCameras.length === 0 ? qsTr("No camera found") : ""
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
                    errorLabel.text = ""
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
            onClicked: page.front = !page.front
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
            text: qsTr("Retake")
            onClicked: page.captured = ""
        }
        Button {
            text: page.acceptText
            onClicked: {
                var path = page.captured
                page.photoTaken(path)
                pageStack.pop()
            }
        }
    }
}
