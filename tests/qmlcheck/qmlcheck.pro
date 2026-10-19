# Loads the installed app QML with the real Sailfish modules inside an SDK
# target, backed by the real (offline) AccountManager. Used by CI
# (ci/sfos-qml-check.sh); not part of the RPM.
TEMPLATE = app
TARGET = qmlcheck
CONFIG += c++11
QT += quick qml dbus multimedia
include(../../app/src/core/core.pri)
INCLUDEPATH += ../../app/src
SOURCES += main.cpp ../../app/src/dbusservice.cpp ../../app/src/voicerecorder.cpp
HEADERS += ../../app/src/dbusservice.h ../../app/src/voicerecorder.h
isEmpty(LIBOSCAR_DIR): LIBOSCAR_DIR = /usr/share/harbour-icqnina/lib
LIBS += -L$$LIBOSCAR_DIR -l:liboscar.so.1
QMAKE_RPATHDIR += $$LIBOSCAR_DIR
