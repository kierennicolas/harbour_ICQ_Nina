import QtQuick 2.6
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    RemorsePopup { id: remorse }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("About ICQ Nina")
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))
            }
        }

        Column {
            id: column
            width: parent.width

            PageHeader { title: qsTr("Settings") }

            SectionHeader { text: qsTr("Account") }

            DetailItem {
                label: account.isIcq ? qsTr("ICQ number") : qsTr("Screen name")
                value: account.hasAccount ? account.uin : qsTr("Not signed in")
            }
            DetailItem {
                label: qsTr("Server")
                value: account.serverProfile === "nina" ? qsTr("NINA (unencrypted)")
                     : account.serverProfile === "custom" ? account.host + ":" + account.port + (account.tls ? " (TLS)" : "")
                     : qsTr("NINA (encrypted)")
            }

            ButtonLayout {
                Button {
                    text: account.hasAccount ? qsTr("Change account") : qsTr("Sign in")
                    onClicked: pageStack.push(Qt.resolvedUrl("SignInDialog.qml"))
                }
                Button {
                    visible: account.hasAccount && account.state !== 0
                    text: qsTr("Go offline")
                    onClicked: account.disconnectAccount()
                }
                Button {
                    visible: account.hasAccount
                    text: qsTr("Forget account")
                    onClicked: remorse.execute(qsTr("Forgetting account"), function () { account.forgetAccount() })
                }
            }

            SectionHeader { text: qsTr("Contacts") }

            BackgroundItem {
                width: parent.width
                height: Theme.itemSizeSmall
                onClicked: pageStack.push(Qt.resolvedUrl("PrivacyPage.qml"))
                Label {
                    x: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: account.blockedContacts.length > 0
                          ? qsTr("Privacy and blocking (%n blocked)", "", account.blockedContacts.length)
                          : qsTr("Privacy and blocking")
                }
            }
            BackgroundItem {
                width: parent.width
                height: Theme.itemSizeSmall
                onClicked: pageStack.push(Qt.resolvedUrl("GroupsPage.qml"))
                Label {
                    x: Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: qsTr("Groups")
                }
            }

            SectionHeader { text: qsTr("Files") }

            TextSwitch {
                checked: account.proxyTransfers
                automaticCheck: false
                onClicked: account.proxyTransfers = !account.proxyTransfers
                text: qsTr("Send files through the server's relay")
                description: qsTr("Uses the rendezvous proxy (%1) from the start. Phones are rarely reachable directly, so this is the reliable choice; turn it off to try a direct connection first, for example on the same Wi-Fi.").arg(account.fileProxyHost)
            }

            DetailItem {
                label: qsTr("Received files")
                value: account.transfers.downloadDir
            }

            SectionHeader { text: qsTr("Connection") }

            TextSwitch {
                checked: account.stayAwake
                automaticCheck: false
                onClicked: account.stayAwake = !account.stayAwake
                text: qsTr("Stay connected with the screen off")
                description: qsTr("Keeps the phone from deep sleep while you are online, so messages arrive reliably. Uses more battery. Without it the app still wakes every few minutes to keep the session alive.")
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                visible: account.lastError.length > 0
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                font.pixelSize: Theme.fontSizeExtraSmall
                text: qsTr("Last connection message: %1").arg(account.lastError)
            }
        }
        VerticalScrollDecorator {}
    }
}
