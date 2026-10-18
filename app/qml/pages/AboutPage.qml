import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader { title: qsTr("About") }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.iconSizeExtraLarge
                height: width
                sourceSize.width: width
                sourceSize.height: height
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-icqnina.png"
            }

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("ICQ Nina %1").arg(appVersion)
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                text: qsTr("An ICQ and AIM client for Sailfish OS that connects to NINA (nina.chat), the revival service for the classic OSCAR protocol.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeSmall
                text: qsTr("Unofficial: not made or endorsed by NINA, ICQ or AOL. ICQ on NINA is still in beta, so some features (offline messages, authorization requests) depend on what the server supports.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryColor
                font.pixelSize: Theme.fontSizeExtraSmall
                textFormat: Text.StyledText
                linkColor: Theme.highlightColor
                onLinkActivated: Qt.openUrlExternally(link)
                text: qsTr("Free software under the GNU GPL 3 or later. The OSCAR protocol library comes from KDE's Kopete messenger (GPL 2 or later). "
                           + "Source: <a href=\"https://github.com/kierennicolas/harbour_ICQ_Nina\">github.com/kierennicolas/harbour_ICQ_Nina</a>")
            }
        }
        VerticalScrollDecorator {}
    }
}
