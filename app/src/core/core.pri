# The app's pure-Qt core, shared by the Sailfish app and the desktop tests.
CORE = $$PWD
LIBOSCAR = $$PWD/../../../liboscar
QT += network gui qml sql
INCLUDEPATH += $$CORE $$LIBOSCAR $$LIBOSCAR/tasks $$LIBOSCAR/utils $$LIBOSCAR/compat
SOURCES += $$CORE/accountmanager.cpp $$CORE/chatmodel.cpp $$CORE/contactmodel.cpp $$CORE/historystore.cpp \
    $$CORE/linkupload.cpp $$CORE/searchmodel.cpp $$CORE/transfermanager.cpp $$CORE/avatars.cpp \
    $$CORE/medialink.cpp $$CORE/mediamanager.cpp $$CORE/stickerstore.cpp $$CORE/emoji.cpp
HEADERS += $$CORE/accountmanager.h $$CORE/chatmodel.h $$CORE/contactmodel.h $$CORE/historystore.h \
    $$CORE/linkupload.h $$CORE/searchmodel.h $$CORE/transfermanager.h $$CORE/avatars.h \
    $$CORE/medialink.h $$CORE/mediamanager.h $$CORE/stickerstore.h $$CORE/emoji.h
