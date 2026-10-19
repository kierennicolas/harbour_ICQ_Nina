import QtQuick 2.6
import Sailfish.Silica 1.0
import "Presence.js" as Presence

// A contact's profile picture (round), or their initial on a colour of
// their own when they have none; optionally with a presence dot.
Item {
    id: avatar

    property string source        // round picture (file URL) or ""
    property string name
    property string uin
    property int presence: -1     // -1: no dot
    property real size: Theme.iconSizeMedium

    width: size
    height: size

    readonly property color tint: {
        // A stable colour from the number or name.
        var s = uin.length > 0 ? uin : name
        var h = 0
        for (var i = 0; i < s.length; ++i)
            h = (h * 31 + s.charCodeAt(i)) % 360
        return Qt.hsla(h / 360, 0.45, 0.45, 1)
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        visible: picture.status !== Image.Ready
        color: avatar.tint

        Label {
            anchors.centerIn: parent
            font.pixelSize: avatar.size * 0.45
            font.bold: true
            color: "white"
            text: {
                var n = avatar.name.trim()
                if (n.length === 0)
                    return "?"
                var c = n.charCodeAt(0)
                // Keep surrogate pairs (emoji) whole.
                return (c >= 0xD800 && c <= 0xDBFF ? n.substr(0, 2) : n.charAt(0)).toUpperCase()
            }
        }
    }

    Image {
        id: picture
        anchors.fill: parent
        source: avatar.source
        sourceSize.width: Math.ceil(avatar.size)
        sourceSize.height: Math.ceil(avatar.size)
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        smooth: true
        visible: status === Image.Ready
    }

    Rectangle {
        visible: avatar.presence >= 0
        width: Math.max(Theme.paddingMedium, avatar.size * 0.32)
        height: width
        radius: width / 2
        anchors {
            right: parent.right
            bottom: parent.bottom
        }
        color: Theme.highlightDimmerColor

        PresenceDot {
            anchors.centerIn: parent
            width: parent.width * 0.72
            presence: avatar.presence
        }
    }
}
