import QtQuick 2.6
import Sailfish.Silica 1.0
import "Presence.js" as Presence

// A small presence indicator: filled for online states, a ring when offline
// or invisible.
Rectangle {
    property int presence: Presence.Offline

    width: Theme.paddingLarge
    height: width
    radius: width / 2
    color: Presence.color(presence)
    border.width: (presence === Presence.Offline || presence === Presence.Invisible) ? Math.max(1, Theme.paddingSmall / 3) : 0
    border.color: presence === Presence.Invisible ? Presence.color(presence) : Theme.secondaryColor
}
