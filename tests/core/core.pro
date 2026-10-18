# Desktop tests for the app core against a local Open OSCAR Server.
TEMPLATE = app
TARGET = tst_core
CONFIG += c++11 console testcase
CONFIG -= app_bundle
QT += testlib
include(../../app/src/core/core.pri)
isEmpty(LIBOSCAR_BUILD): LIBOSCAR_BUILD = $$OUT_PWD/../../liboscar
LIBS += -L$$LIBOSCAR_BUILD -loscar
QMAKE_RPATHDIR += $$LIBOSCAR_BUILD
SOURCES += tst_core.cpp
