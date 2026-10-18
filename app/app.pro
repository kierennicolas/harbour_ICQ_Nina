# ICQ Nina, the Sailfish OS app.
TARGET = harbour-icqnina
CONFIG += sailfishapp c++11
QT += dbus

isEmpty(VERSION): VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

include(src/core/core.pri)

# liboscar is built alongside and shipped privately in
# /usr/share/harbour-icqnina/lib (sailfishapp.prf adds that RPATH).
LIBS += -L$$OUT_PWD/../liboscar -loscar

SOURCES += src/main.cpp src/dbusservice.cpp
HEADERS += src/dbusservice.h

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
    qml/components/Presence.js

# Licence texts go with the app (Harbour allows files only under
# /usr/share/harbour-icqnina).
licenses.files = ../LICENSE ../liboscar/COPYING ../liboscar/COPYING.LIB
licenses.path = /usr/share/$${TARGET}/licenses
INSTALLS += licenses

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
