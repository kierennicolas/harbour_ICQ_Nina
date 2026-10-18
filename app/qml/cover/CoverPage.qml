import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"
import "../components/Presence.js" as Presence

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingSmall

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.iconSizeLarge
            height: width
            sourceSize.width: width
            sourceSize.height: height
            source: "/usr/share/icons/hicolor/172x172/apps/harbour-icqnina.png"
            opacity: account.state === 2 ? 1.0 : 0.5
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: account.unreadTotal > 0
            text: account.unreadTotal
            font.pixelSize: Theme.fontSizeHuge
            color: Theme.highlightColor
        }
        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: account.unreadTotal > 0
            text: qsTr("unread", "", account.unreadTotal)
            font.pixelSize: Theme.fontSizeSmall
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.paddingSmall
            PresenceDot {
                anchors.verticalCenter: parent.verticalCenter
                presence: account.state === 2 ? account.status : Presence.Offline
            }
            Label {
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: !account.hasAccount ? qsTr("Not signed in")
                    : account.state === 1 ? qsTr("Connecting")
                    : account.state === 2 ? Presence.name(account.status)
                    : qsTr("Offline")
            }
        }
    }
}
