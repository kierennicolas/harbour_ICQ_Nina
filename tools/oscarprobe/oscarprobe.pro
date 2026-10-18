# oscarprobe: command-line login/messaging probe built on liboscar.
# Used by CI against a local Open OSCAR Server and against NINA.
TEMPLATE = app
TARGET = oscarprobe
CONFIG += c++11 console
CONFIG -= app_bundle
QT += core network gui xml

LIBOSCAR = $$PWD/../../liboscar
INCLUDEPATH += $$LIBOSCAR $$LIBOSCAR/tasks $$LIBOSCAR/utils $$LIBOSCAR/compat
isEmpty(LIBOSCAR_BUILD): LIBOSCAR_BUILD = $$OUT_PWD/../../liboscar
LIBS += -L$$LIBOSCAR_BUILD -loscar
QMAKE_RPATHDIR += $$LIBOSCAR_BUILD

SOURCES += main.cpp
