# ICQ Nina, the Sailfish OS app.
TARGET = harbour-icqnina
CONFIG += sailfishapp c++11
QT += dbus multimedia

isEmpty(VERSION): VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

include(src/core/core.pri)

# liboscar is built alongside and shipped privately in
# /usr/share/harbour-icqnina/lib (sailfishapp.prf adds that RPATH).
LIBS += -L$$OUT_PWD/../liboscar -loscar

SOURCES += src/main.cpp src/dbusservice.cpp src/voicerecorder.cpp
HEADERS += src/dbusservice.h src/voicerecorder.h

DISTFILES += \
    harbour-icqnina.desktop \
    qml/harbour-icqnina.qml \
    qml/cover/CoverPage.qml \
    qml/pages/ContactsPage.qml \
    qml/pages/ConversationPage.qml \
    qml/pages/SignInDialog.qml \
    qml/pages/StatusPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/AboutPage.qml \
    qml/components/PresenceDot.qml \
    qml/components/Presence.js \
    qml/components/Avatar.qml \
    qml/components/MediaBubble.qml \
    qml/components/EmojiPanel.qml \
    qml/pages/ProfilePage.qml \
    qml/pages/CameraPage.qml \
    qml/pages/ImageViewerPage.qml \
    qml/pages/PhotoSendDialog.qml

# Licence texts go with the app (Harbour allows files only under
# /usr/share/harbour-icqnina).
licenses.files = ../LICENSE ../liboscar/COPYING ../liboscar/COPYING.LIB
licenses.path = /usr/share/$${TARGET}/licenses
INSTALLS += licenses

# The default new-message sound (original, synthesized for this app).
sounds.files = sounds/icq-nina-message.wav
sounds.path = /usr/share/$${TARGET}/sounds
INSTALLS += sounds

# The app's own sticker pack and the emoji pictures (Twemoji, CC-BY 4.0).
stickers.files = stickers/*.png
stickers.path = /usr/share/$${TARGET}/stickers
emoji.files = emoji/*.png emoji/emoji.json emoji/LICENSE-GRAPHICS
emoji.path = /usr/share/$${TARGET}/emoji
INSTALLS += stickers emoji

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
