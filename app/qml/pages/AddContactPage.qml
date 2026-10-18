import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// Find someone by ICQ number (the directory search NINA supports) and add
// them, or add the number directly. For AIM accounts, enter a screen name.
Page {
    id: page
    allowedOrientations: Orientation.All

    // The field lives in the list header; its text is mirrored here.
    property string queryText
    property string failureText
    property bool searched: false
    readonly property string digits: queryText.replace(/[^0-9]/g, "")
    readonly property bool canSearch: account.state === 2 && (account.isIcq ? digits.length >= 5 : queryText.trim().length > 0)

    function search() {
        if (!canSearch)
            return
        failureText = ""
        searched = true
        account.searchByUin(digits)
    }

    function add(uin, name, authRequired) {
        pageStack.push(Qt.resolvedUrl("AddContactDialog.qml"),
                       { uin: uin, name: name || "", authRequired: authRequired || false })
    }

    Component.onDestruction: account.clearSearch()

    Connections {
        target: account
        onContactEditFailed: page.failureText = message
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: account.isIcq ? account.searchResults : null

        header: Column {
            width: list.width

            PageHeader { title: qsTr("Add contact") }

            TextField {
                width: parent.width
                focus: true
                label: account.isIcq ? qsTr("ICQ number") : qsTr("Screen name")
                placeholderText: label
                inputMethodHints: account.isIcq ? Qt.ImhDigitsOnly : Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                onTextChanged: {
                    page.queryText = text
                    page.searched = false
                }
                EnterKey.enabled: page.canSearch
                EnterKey.iconSource: account.isIcq ? "image://theme/icon-m-search" : "image://theme/icon-m-enter-next"
                EnterKey.onClicked: account.isIcq ? page.search() : page.add(page.queryText.trim(), "", false)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: account.isIcq ? qsTr("Search") : qsTr("Add")
                enabled: page.canSearch && !account.searching
                onClicked: account.isIcq ? page.search() : page.add(page.queryText.trim(), "", false)
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: account.searching
                visible: running
                size: BusyIndicatorSize.Medium
            }

            Label {
                visible: text.length > 0
                text: page.failureText
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                color: Theme.errorColor
                font.pixelSize: Theme.fontSizeSmall
            }

            // A number can be added without finding it first.
            BackgroundItem {
                visible: account.isIcq && page.digits.length >= 5 && !account.searching
                width: parent.width
                height: Theme.itemSizeSmall
                onClicked: page.add(page.digits, "", false)
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    anchors.verticalCenter: parent.verticalCenter
                    truncationMode: TruncationMode.Fade
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: qsTr("Add %1 directly").arg(page.digits)
                }
            }

            SectionHeader {
                visible: list.count > 0
                text: qsTr("Found")
            }
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeMedium
            onClicked: page.add(model.uin, model.name, model.authRequired)

            PresenceDot {
                id: dot
                presence: model.online ? 0 : 7
                anchors {
                    left: parent.left
                    leftMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
            }

            Column {
                anchors {
                    left: dot.right
                    leftMargin: Theme.paddingMedium
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                    verticalCenter: parent.verticalCenter
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: model.name
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                    text: {
                        var parts = [model.uin]
                        parts.push(model.online ? qsTr("online") : qsTr("offline"))
                        if (model.authRequired)
                            parts.push(qsTr("asks to approve"))
                        if (account.isContact(model.uin))
                            parts.push(qsTr("already in your list"))
                        return parts.join(" · ")
                    }
                }
            }
        }

        ViewPlaceholder {
            enabled: account.isIcq && page.searched && !account.searching && list.count === 0
            text: qsTr("Nobody found")
            hintText: qsTr("You can still add the number directly")
        }

        VerticalScrollDecorator {}
    }
}
