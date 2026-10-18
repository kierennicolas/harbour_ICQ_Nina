import QtQuick 2.6
import Sailfish.Silica 1.0
import "../components"

// Find people in the ICQ directory (by number, nickname, name or email) and
// add them. For AIM accounts there is no directory: enter a screen name.
Page {
    id: page
    allowedOrientations: Orientation.All

    // 0 number, 1 nickname, 2 name, 3 email
    property int mode: 0
    // The fields live in the list header; their text is mirrored here.
    property string queryText
    property string lastNameText
    property string failureText
    readonly property string digits: queryText.replace(/[^0-9]/g, "")
    readonly property bool canSearch: account.state === 2 && (
        mode === 0 ? digits.length >= 5 :
        mode === 2 ? (queryText.trim().length > 0 || lastNameText.trim().length > 0) :
        mode === 3 ? queryText.indexOf("@") > 0 : queryText.trim().length > 0)
    property bool searched: false

    function search() {
        if (!canSearch)
            return
        searched = true
        if (!account.isIcq)
            return
        failureText = ""
        if (mode === 0) account.searchByUin(digits)
        else if (mode === 1) account.searchByNickname(queryText)
        else if (mode === 2) account.searchByName(queryText, lastNameText)
        else account.searchByEmail(queryText)
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

            ComboBox {
                visible: account.isIcq
                label: qsTr("Find by")
                currentIndex: page.mode
                menu: ContextMenu {
                    MenuItem { text: qsTr("ICQ number") }
                    MenuItem { text: qsTr("Nickname") }
                    MenuItem { text: qsTr("Name") }
                    MenuItem { text: qsTr("Email") }
                }
                onCurrentIndexChanged: {
                    page.mode = currentIndex
                    page.searched = false
                    account.clearSearch()
                }
            }

            TextField {
                id: query
                width: parent.width
                focus: true
                label: !account.isIcq ? qsTr("Screen name")
                     : page.mode === 0 ? qsTr("ICQ number")
                     : page.mode === 1 ? qsTr("Nickname")
                     : page.mode === 2 ? qsTr("First name")
                     : qsTr("Email address")
                placeholderText: label
                inputMethodHints: page.mode === 0 && account.isIcq ? Qt.ImhDigitsOnly
                                : page.mode === 3 ? Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                                : Qt.ImhNoPredictiveText
                onTextChanged: {
                    page.queryText = text
                    page.searched = false
                }
                EnterKey.enabled: page.canSearch
                EnterKey.iconSource: "image://theme/icon-m-search"
                EnterKey.onClicked: {
                    if (page.mode === 2 && account.isIcq)
                        lastName.forceActiveFocus()
                    else if (account.isIcq)
                        page.search()
                    else
                        page.add(page.queryText.trim(), "", false)
                }
            }

            TextField {
                id: lastName
                visible: page.mode === 2 && account.isIcq
                width: parent.width
                label: qsTr("Last name")
                placeholderText: label
                onTextChanged: page.lastNameText = text
                EnterKey.enabled: page.canSearch
                EnterKey.iconSource: "image://theme/icon-m-search"
                EnterKey.onClicked: page.search()
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
                size: BusySize.Medium
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

            // An ICQ number can be added without finding it first: some
            // servers keep few people in their directory.
            BackgroundItem {
                visible: account.isIcq && page.mode === 0 && page.digits.length >= 5 && !account.searching
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

            Item { width: 1; height: Theme.paddingSmall }
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
                        var full = (model.firstName + " " + model.lastName).trim()
                        if (full.length > 0 && full !== model.name)
                            parts.push(full)
                        if (model.age > 0)
                            parts.push(qsTr("%1 years").arg(model.age))
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
            hintText: page.mode === 0 ? qsTr("You can still add the number directly")
                                      : qsTr("Not everyone is listed in the directory")
        }

        VerticalScrollDecorator {}
    }
}
