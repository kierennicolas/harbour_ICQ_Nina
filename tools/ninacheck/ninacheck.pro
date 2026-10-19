# ninacheck: runs the app's own core (AccountManager) against NINA with the
# CI test account, for checks the protocol probe cannot make (the app's
# profile saving end to end). Prints only "kept or not", never contents.
TEMPLATE = app
TARGET = ninacheck
CONFIG += c++11 console
CONFIG -= app_bundle
QT += testlib
include(../../app/src/core/core.pri)
isEmpty(LIBOSCAR_BUILD): LIBOSCAR_BUILD = $$OUT_PWD/../../liboscar
LIBS += -L$$LIBOSCAR_BUILD -loscar
QMAKE_RPATHDIR += $$LIBOSCAR_BUILD
SOURCES += main.cpp
