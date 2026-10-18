import QtQuick 2.6
import Sailfish.Silica 1.0

Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string uin: account.uin

    canAccept: uinField.text.trim().length > 0 && passwordField.text.length > 0
               && (server.currentIndex !== 2 || hostField.text.trim().length > 0)

    onAccepted: {
        var profiles = ["nina-tls", "nina", "custom"]
        account.signIn(uinField.text, passwordField.text, rememberSwitch.checked,
                       profiles[server.currentIndex], hostField.text,
                       parseInt(portField.text) || 5190, tlsSwitch.checked)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width
            spacing: Theme.paddingSmall

            DialogHeader {
                acceptText: qsTr("Sign in")
            }

            TextField {
                id: uinField
                width: parent.width
                text: dialog.uin
                label: qsTr("ICQ number or AIM screen name")
                placeholderText: label
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.focus = true
            }

            TextField {
                id: passwordField
                width: parent.width
                label: qsTr("Password")
                placeholderText: label
                echoMode: TextInput.Password
                inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.enabled: dialog.canAccept
                EnterKey.onClicked: dialog.accept()
            }

            TextSwitch {
                id: rememberSwitch
                checked: true
                text: qsTr("Remember password")
                description: qsTr("Stored in this app's private folder so it can reconnect by itself.")
            }

            ComboBox {
                id: server
                width: parent.width
                label: qsTr("Server")
                currentIndex: account.serverProfile === "nina" ? 1 : account.serverProfile === "custom" ? 2 : 0
                menu: ContextMenu {
                    MenuItem { text: qsTr("NINA (encrypted)") }
                    MenuItem { text: qsTr("NINA (unencrypted)") }
                    MenuItem { text: qsTr("Other OSCAR server") }
                }
            }

            Column {
                width: parent.width
                visible: server.currentIndex === 2

                TextField {
                    id: hostField
                    width: parent.width
                    text: account.serverProfile === "custom" ? account.host : ""
                    label: qsTr("Server address")
                    placeholderText: label
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText
                }
                TextField {
                    id: portField
                    width: parent.width
                    text: account.serverProfile === "custom" ? account.port : "5190"
                    label: qsTr("Port")
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                TextSwitch {
                    id: tlsSwitch
                    checked: account.serverProfile === "custom" ? account.tls : false
                    text: qsTr("Use SSL/TLS")
                }
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: account.lastError.length > 0
                text: account.lastError
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                textFormat: Text.StyledText
                linkColor: Theme.highlightColor
                onLinkActivated: Qt.openUrlExternally(link)
                text: qsTr("New to NINA? Register a number (choose the account type “UIN”) at "
                           + "<a href=\"https://account.escargot.chat/register/\">account.escargot.chat</a>. "
                           + "If you set a separate legacy password for old clients there, use that one here.")
            }
        }
        VerticalScrollDecorator {}
    }
}
