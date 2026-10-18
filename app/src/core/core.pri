# The app's pure-Qt core, shared by the Sailfish app and the desktop tests.
CORE = $$PWD
LIBOSCAR = $$PWD/../../../liboscar
QT += network gui qml sql
INCLUDEPATH += $$CORE $$LIBOSCAR $$LIBOSCAR/tasks $$LIBOSCAR/utils $$LIBOSCAR/compat
SOURCES += $$CORE/accountmanager.cpp $$CORE/chatmodel.cpp $$CORE/contactmodel.cpp $$CORE/historystore.cpp
HEADERS += $$CORE/accountmanager.h $$CORE/chatmodel.h $$CORE/contactmodel.h $$CORE/historystore.h
