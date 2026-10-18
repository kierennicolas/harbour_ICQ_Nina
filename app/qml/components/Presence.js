.pragma library
// Presence values match AccountManager::Status / ContactModel::Presence.
// Plain ES5 for Qt 5.6.

var Online = 0, Away = 1, NotAvailable = 2, Occupied = 3, DoNotDisturb = 4,
    FreeForChat = 5, Invisible = 6, Offline = 7

// The order the status picker shows them in.
var choices = [Online, FreeForChat, Away, NotAvailable, Occupied, DoNotDisturb, Invisible, Offline]

function name(p) {
    switch (p) {
    case Online: return qsTr("Online")
    case Away: return qsTr("Away")
    case NotAvailable: return qsTr("Not available")
    case Occupied: return qsTr("Occupied")
    case DoNotDisturb: return qsTr("Do not disturb")
    case FreeForChat: return qsTr("Free for chat")
    case Invisible: return qsTr("Invisible")
    default: return qsTr("Offline")
    }
}

function color(p) {
    switch (p) {
    case Online: return "#3ec44c"
    case FreeForChat: return "#26b3c6"
    case Away: return "#f2b632"
    case NotAvailable: return "#f08a2c"
    case Occupied: return "#e05454"
    case DoNotDisturb: return "#c22d2d"
    case Invisible: return "#9a9a9a"
    default: return "transparent"
    }
}
