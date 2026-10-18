import QtQuick 2.6
import Sailfish.Silica 1.0

// One line of text, e.g. a contact's name or a group name. The caller
// connects to accepted() and reads value.
Dialog {
    id: dialog
    allowedOrientations: Orientation.All

    property string title
    property string label
    property string placeholder
    property alias value: field.text
    property string acceptText: qsTr("Save")
    property bool allowEmpty: false

    canAccept: allowEmpty || field.text.trim().length > 0

    Column {
        width: parent.width

        DialogHeader {
            title: dialog.title
            acceptText: dialog.acceptText
        }

        TextField {
            id: field
            width: parent.width
            label: dialog.label
            placeholderText: dialog.placeholder.length > 0 ? dialog.placeholder : dialog.label
            focus: true
            EnterKey.enabled: dialog.canAccept
            EnterKey.iconSource: "image://theme/icon-m-enter-accept"
            EnterKey.onClicked: dialog.accept()
        }
    }
}
