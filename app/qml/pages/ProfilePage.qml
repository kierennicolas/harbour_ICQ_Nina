import QtQuick 2.6
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../components"

// Your own profile: picture and the details others see in the ICQ directory.
Dialog {
    id: dialog
    allowedOrientations: Orientation.All
    canAccept: account.state === 2 && account.isIcq && !account.savingProfile

    // Filled from what the directory says, and again when a newer answer
    // arrives, until you change something (then your typing is kept).
    property bool loaded: false
    property bool filling: false
    property bool touched: false
    function touch() { if (!filling) touched = true }
    // Tested against NINA (October 2026): it keeps nickname, names, city,
    // region and "about me", but not email, home page, gender or birthday.
    // After saving, the app also checks what the server kept.
    readonly property bool onNina: account.serverProfile.indexOf("nina") === 0
    property var birthday: null      // Date or null
    property string pickedPicture

    function fill(info) {
        if (touched || !info || Object.keys(info).length <= 1)
            return
        filling = true
        nickname.text = info.nickname || ""
        firstName.text = info.firstName || ""
        lastName.text = info.lastName || ""
        email.text = info.email || ""
        city.text = info.city || ""
        region.text = info.state || ""
        homepage.text = info.homepage || ""
        gender.currentIndex = info.gender === 1 ? 1 : info.gender === 2 ? 2 : 0
        birthday = info.birthday ? new Date(info.birthday) : null
        about.text = info.about || ""
        loaded = true
        filling = false
    }

    function choosePicture() {
        pageStack.push(picturePicker)
    }

    function takePicture() {
        var cam = pageStack.push(Qt.resolvedUrl("CameraPage.qml"),
                                 { savePath: account.media.newFilePath("camera", "jpg"), acceptText: qsTr("Use") })
        cam.photoTaken.connect(function(path) { dialog.usePicture(path, true) })
    }

    function usePicture(path, temporary) {
        if (!account.setOwnPicture(path))
            info.text = qsTr("That picture cannot be used.")
        else
            info.text = account.state === 2 ? qsTr("Picture changed.") : qsTr("Picture changed; it is sent when you are online.")
        if (temporary)
            account.media.discard(path)
    }

    Component.onCompleted: {
        fill(account.ownProfile)
        if (account.state === 2 && account.isIcq)
            account.requestUserInfo(account.uin)
    }

    onStatusChanged: {
        if (status === PageStatus.Active && pickedPicture.length > 0) {
            var p = pickedPicture
            pickedPicture = ""
            usePicture(p, false)
        }
    }

    onAccepted: {
        var f = {
            nickname: nickname.text, firstName: firstName.text, lastName: lastName.text,
            email: email.text, city: city.text, state: region.text, homepage: homepage.text,
            gender: gender.currentIndex, about: about.text
        }
        if (birthday)
            f.birthday = birthday
        account.saveProfile(f)
    }

    Connections {
        target: account
        onUserInfoReceived: if (uin === account.uin) dialog.fill(info)
    }

    Component {
        id: picturePicker
        ImagePickerPage {
            title: qsTr("Profile picture")
            onSelectedContentPropertiesChanged: {
                if (selectedContentProperties && selectedContentProperties.filePath)
                    dialog.pickedPicture = selectedContentProperties.filePath
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column
            width: parent.width

            DialogHeader {
                acceptText: qsTr("Save")
                title: qsTr("My profile")
            }

            Avatar {
                anchors.horizontalCenter: parent.horizontalCenter
                size: Theme.itemSizeHuge * 1.4
                source: account.ownPicture
                name: nickname.text.length > 0 ? nickname.text : account.uin
                uin: account.uin
            }

            Item { width: 1; height: Theme.paddingMedium }

            Flow {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width - 2 * Theme.horizontalPageMargin, implicitWidth)
                spacing: Theme.paddingMedium
                Button {
                    text: qsTr("Choose")
                    onClicked: dialog.choosePicture()
                }
                Button {
                    text: qsTr("Take photo")
                    onClicked: dialog.takePicture()
                }
                Button {
                    visible: account.ownPicture.length > 0
                    text: qsTr("Remove")
                    onClicked: account.removeOwnPicture()
                }
            }

            Label {
                id: info
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                topPadding: Theme.paddingSmall
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.highlightColor
            }

            Label {
                visible: account.state !== 2 || !account.isIcq
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                topPadding: Theme.paddingMedium
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: !account.isIcq ? qsTr("Directory details are only for ICQ numbers.")
                                     : qsTr("Go online to change your details.")
            }

            SectionHeader { text: qsTr("About you") }

            TextField {
                id: nickname
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("Nickname")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: firstName.focus = true
            }
            TextField {
                id: firstName
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("First name")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: lastName.focus = true
            }
            TextField {
                id: lastName
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("Last name")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: email.focus = true
            }
            TextField {
                id: email
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("Email (shown to others)")
                placeholderText: qsTr("Email")
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: city.focus = true
            }
            Label {
                visible: dialog.onNina
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("NINA does not keep email addresses at the moment.")
            }
            TextField {
                id: city
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("City")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: region.focus = true
            }
            TextField {
                id: region
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("State or region")
                placeholderText: label
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: homepage.focus = true
            }
            TextField {
                id: homepage
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("Home page")
                placeholderText: label
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                EnterKey.iconSource: "image://theme/icon-m-enter-close"
                EnterKey.onClicked: focus = false
            }

            ComboBox {
                id: gender
                width: parent.width
                onCurrentIndexChanged: dialog.touch()
                label: qsTr("Gender")
                menu: ContextMenu {
                    MenuItem { text: qsTr("Not said") }
                    MenuItem { text: qsTr("Female") }
                    MenuItem { text: qsTr("Male") }
                }
            }

            ValueButton {
                label: qsTr("Birthday")
                value: dialog.birthday ? Format.formatDate(dialog.birthday, Formatter.DateLong) : qsTr("Not said")
                onClicked: {
                    var d = pageStack.push("Sailfish.Silica.DatePickerDialog",
                                           { date: dialog.birthday ? dialog.birthday : new Date(1990, 0, 1) })
                    d.accepted.connect(function() { dialog.birthday = d.date; dialog.touch() })
                }
            }
            Button {
                visible: dialog.birthday !== null
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Clear birthday")
                onClicked: { dialog.birthday = null; dialog.touch() }
            }
            Label {
                visible: dialog.onNina
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("NINA does not keep the home page, gender or birthday at the moment.")
            }

            TextArea {
                id: about
                width: parent.width
                onTextChanged: dialog.touch()
                label: qsTr("About me")
                placeholderText: label
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Anyone can look these up by your ICQ number.")
            }
        }

        VerticalScrollDecorator {}
    }
}
