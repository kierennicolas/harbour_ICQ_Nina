import QtQuick 2.6
import Sailfish.Silica 1.0

Dialog {
    property string uin
    property string name
    property string reason

    allowedOrientations: Orientation.All
    onAccepted: account.answerAuthorization(uin, true)
    onRejected: account.answerAuthorization(uin, false)

    Column {
        width: parent.width
        spacing: Theme.paddingLarge

        DialogHeader {
            acceptText: qsTr("Allow")
            cancelText: qsTr("Deny")
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            text: qsTr("%1 wants to add you to their contact list").arg(name)
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            visible: reason.length > 0
            wrapMode: Text.Wrap
            text: reason
        }

        Label {
            x: Theme.horizontalPageMargin
            width: parent.width - 2 * x
            wrapMode: Text.Wrap
            color: Theme.secondaryColor
            font.pixelSize: Theme.fontSizeSmall
            text: qsTr("If you allow it, %1 will see when you are online.").arg(uin)
        }
    }
}
