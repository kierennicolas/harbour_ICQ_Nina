import QtQuick 2.6
import Sailfish.Silica 1.0

// Shows a picked photo before it is sent.
Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string filePath
    property string name

    DialogHeader {
        id: header
        acceptText: qsTr("Send")
        title: qsTr("Photo to %1").arg(dialog.name)
    }

    Image {
        anchors {
            top: header.bottom
            left: parent.left
            right: parent.right
            bottom: note.top
            margins: Theme.horizontalPageMargin
        }
        source: dialog.filePath.length > 0 ? "file://" + dialog.filePath.split("/").map(encodeURIComponent).join("/") : ""
        fillMode: Image.PreserveAspectFit
        autoTransform: true
        asynchronous: true
        sourceSize.width: 1600
        sourceSize.height: 1600
    }

    Label {
        id: note
        anchors {
            bottom: parent.bottom
            bottomMargin: Theme.paddingLarge
            left: parent.left
            right: parent.right
            leftMargin: Theme.horizontalPageMargin
            rightMargin: Theme.horizontalPageMargin
        }
        wrapMode: Text.Wrap
        font.pixelSize: Theme.fontSizeExtraSmall
        color: Theme.secondaryHighlightColor
        text: qsTr("Sent as a smaller copy (location and other details removed) through a temporary file host; the link works for 3 days, and %1 keeps the photo if they use ICQ Nina.").arg(dialog.name)
    }
}
