import QtQuick 2.6
import Sailfish.Silica 1.0

// Emoji and stickers, docked above the message field. Tapping an emoji puts
// it in the message; tapping a sticker sends it.
Item {
    id: panel

    property bool stickers: false
    signal emojiPicked(string text)
    signal stickerPicked(string stickerId)
    signal addStickerRequested()
    signal removeStickerRequested(string stickerId)

    // Emoji of all categories in one list; where each category starts.
    readonly property var emojiList: {
        var out = []
        var cats = emoji.categories
        for (var i = 0; i < cats.length; ++i)
            for (var j = 0; j < cats[i].items.length; ++j)
                out.push(cats[i].items[j])
        return out
    }
    readonly property var categoryStarts: {
        var out = []
        var n = 0
        var cats = emoji.categories
        for (var i = 0; i < cats.length; ++i) {
            out.push(n)
            n += cats[i].items.length
        }
        return out
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightDimmerColor, 0.6)
    }

    Row {
        id: tabs
        width: parent.width
        height: Theme.itemSizeExtraSmall

        BackgroundItem {
            width: Theme.itemSizeMedium
            height: parent.height
            highlighted: down || !panel.stickers
            onClicked: panel.stickers = false
            Image {
                anchors.centerIn: parent
                width: Theme.iconSizeSmallPlus
                height: width
                sourceSize.width: width
                source: emoji.urlOf("😀")
            }
        }
        BackgroundItem {
            width: Theme.itemSizeMedium
            height: parent.height
            highlighted: down || panel.stickers
            onClicked: panel.stickers = true
            Image {
                anchors.centerIn: parent
                width: Theme.iconSizeSmallPlus
                height: width
                sourceSize.width: 64
                source: account.media.stickers.urlOf("hello")
            }
        }

        // Emoji categories: jump there.
        Repeater {
            model: panel.stickers ? 0 : emoji.categories.length
            BackgroundItem {
                width: (tabs.width - 2 * Theme.itemSizeMedium) / Math.max(1, emoji.categories.length)
                height: tabs.height
                onClicked: emojiGrid.positionViewAtIndex(panel.categoryStarts[index], GridView.Beginning)
                Image {
                    anchors.centerIn: parent
                    width: Theme.iconSizeExtraSmall
                    height: width
                    sourceSize.width: width
                    opacity: 0.8
                    source: emoji.categories[index].items[0].url
                }
            }
        }
    }

    SilicaGridView {
        id: emojiGrid
        visible: !panel.stickers
        anchors {
            top: tabs.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        cellWidth: width / Math.max(6, Math.floor(width / (Theme.iconSizeMedium + Theme.paddingMedium)))
        cellHeight: cellWidth
        model: panel.emojiList
        delegate: BackgroundItem {
            width: emojiGrid.cellWidth
            height: emojiGrid.cellHeight
            onClicked: panel.emojiPicked(modelData.text)
            Image {
                anchors.centerIn: parent
                width: parent.width * 0.62
                height: width
                sourceSize.width: 72
                asynchronous: true
                source: modelData.url
            }
        }
        VerticalScrollDecorator {}
    }

    SilicaGridView {
        id: stickerGrid
        visible: panel.stickers
        anchors {
            top: tabs.bottom
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
        clip: true
        cellWidth: width / Math.max(3, Math.floor(width / (Theme.itemSizeLarge * 1.4)))
        cellHeight: cellWidth
        model: account.media.stickers
        delegate: BackgroundItem {
            width: stickerGrid.cellWidth
            height: stickerGrid.cellHeight
            onClicked: panel.stickerPicked(model.stickerId)
            onPressAndHold: if (!model.bundled) panel.removeStickerRequested(model.stickerId)
            Image {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.paddingMedium
                height: width
                sourceSize.width: 256
                asynchronous: true
                fillMode: Image.PreserveAspectFit
                source: model.url
            }
        }
        footer: BackgroundItem {
            width: stickerGrid.width
            height: Theme.itemSizeMedium
            onClicked: panel.addStickerRequested()
            Row {
                anchors.centerIn: parent
                spacing: Theme.paddingMedium
                Image {
                    anchors.verticalCenter: parent.verticalCenter
                    source: "image://theme/icon-m-add?" + Theme.primaryColor
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Add a sticker from your pictures")
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
        VerticalScrollDecorator {}
    }
}
