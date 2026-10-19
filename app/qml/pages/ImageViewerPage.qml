import QtQuick 2.6
import Sailfish.Silica 1.0

// A photo from a conversation, full screen: pinch or double-tap to zoom,
// pull down to save it to Pictures or copy its link.
Page {
    id: page
    allowedOrientations: Orientation.All
    backNavigation: !zoomed

    property string source
    property string filePath
    property string link
    property bool profilePicture: false
    readonly property bool zoomed: flick.contentWidth > flick.width + 1

    function fit() {
        flick.contentWidth = flick.width
        flick.contentHeight = flick.height
    }

    onWidthChanged: fit()
    onHeightChanged: fit()

    Rectangle {
        anchors.fill: parent
        color: "black"
    }

    SilicaFlickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: height
        clip: true

        PullDownMenu {
            visible: !page.zoomed
            MenuItem {
                visible: page.filePath.length > 0 && !page.profilePicture
                text: qsTr("Save to Pictures")
                onClicked: {
                    var saved = account.media.saveToPictures(page.filePath)
                    info.text = saved.length > 0 ? qsTr("Saved in Pictures/ICQ Nina") : account.media.lastError()
                    infoTimer.restart()
                }
            }
            MenuItem {
                visible: page.link.length > 0
                text: qsTr("Copy link")
                onClicked: Clipboard.text = page.link.split("#")[0]
            }
        }

        PinchArea {
            width: Math.max(flick.contentWidth, flick.width)
            height: Math.max(flick.contentHeight, flick.height)
            property real startWidth
            property real startHeight
            onPinchStarted: {
                startWidth = flick.contentWidth
                startHeight = flick.contentHeight
            }
            onPinchUpdated: {
                var scale = Math.max(1, Math.min(5, pinch.scale * startWidth / flick.width))
                flick.resizeContent(flick.width * scale, flick.height * scale, pinch.center)
            }
            onPinchFinished: flick.returnToBounds()

            Image {
                id: image
                width: flick.contentWidth
                height: flick.contentHeight
                source: page.source
                fillMode: Image.PreserveAspectFit
                asynchronous: true
                smooth: !flick.moving
                sourceSize.width: 2048
                sourceSize.height: 2048

                MouseArea {
                    anchors.fill: parent
                    onDoubleClicked: {
                        if (page.zoomed)
                            page.fit()
                        else
                            flick.resizeContent(flick.width * 2.5, flick.height * 2.5, Qt.point(mouse.x, mouse.y))
                    }
                }
            }
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: image.status === Image.Loading
        size: BusyIndicatorSize.Large
    }

    Label {
        anchors.centerIn: parent
        visible: image.status === Image.Error
        color: Theme.secondaryColor
        text: qsTr("Cannot show this picture")
    }

    Label {
        id: info
        visible: text.length > 0
        anchors {
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge
            horizontalCenter: parent.horizontalCenter
        }
        color: "white"
        font.pixelSize: Theme.fontSizeSmall
    }

    Timer {
        id: infoTimer
        interval: 3000
        onTriggered: info.text = ""
    }
}
