# liboscar: the OSCAR (ICQ/AIM) protocol library from KDE's Kopete,
# forked for harbour_icq_nina. Builds with plain Qt 5 (5.6+) and qmake; the KDE
# APIs it used are replaced by small shims in compat/.
# Licence: mixed LGPL-2+/GPL-2+ file headers, so GPL-2.0-or-later as a whole
# (see README.md, COPYING and COPYING.LIB).

TEMPLATE = lib
TARGET = oscar
VERSION = 1.0.0
CONFIG += c++11 shared
QT += core network gui xml

DEFINES += MAKE_OSCAR_LIB
INCLUDEPATH += $$PWD $$PWD/tasks $$PWD/utils $$PWD/compat

SOURCES += \
    oscarutils.cpp \
    client.cpp \
    inputprotocolbase.cpp \
    coreprotocol.cpp \
    flapprotocol.cpp \
    snacprotocol.cpp \
    oftprotocol.cpp \
    ofttransfer.cpp \
    transfer.cpp \
    oscarclientstream.cpp \
    safedelete.cpp \
    stream.cpp \
    buffer.cpp \
    rateclassmanager.cpp \
    contactmanager.cpp \
    contact.cpp \
    rateclass.cpp \
    connection.cpp \
    userdetails.cpp \
    oscartypeclasses.cpp \
    oscarmessage.cpp \
    icquserinfo.cpp \
    oscarsettings.cpp \
    connectionhandler.cpp \
    oscarguid.cpp \
    oscarmessageplugin.cpp \
    oftmetatransfer.cpp \
    xtrazxawayservice.cpp \
    xtrazxservice.cpp \
    xtrazxtraznotify.cpp \
    xtrazxrandomizerservice.cpp \
    filetransferhandler.cpp \
    chatroomhandler.cpp \
    tasks/task.cpp \
    tasks/logintask.cpp \
    tasks/oscarlogintask.cpp \
    tasks/closeconnectiontask.cpp \
    tasks/serverversionstask.cpp \
    tasks/rateinfotask.cpp \
    tasks/errortask.cpp \
    tasks/locationrightstask.cpp \
    tasks/profiletask.cpp \
    tasks/blmlimitstask.cpp \
    tasks/servicesetuptask.cpp \
    tasks/icbmparamstask.cpp \
    tasks/prmparamstask.cpp \
    tasks/ssiparamstask.cpp \
    tasks/ssilisttask.cpp \
    tasks/ssiactivatetask.cpp \
    tasks/clientreadytask.cpp \
    tasks/senddcinfotask.cpp \
    tasks/sendidletimetask.cpp \
    tasks/ownuserinfotask.cpp \
    tasks/onlinenotifiertask.cpp \
    tasks/ssimodifytask.cpp \
    tasks/messagereceivertask.cpp \
    tasks/messageacktask.cpp \
    tasks/sendmessagetask.cpp \
    tasks/icqtask.cpp \
    tasks/offlinemessagestask.cpp \
    tasks/icqofflinemessagestask.cpp \
    tasks/ssiauthtask.cpp \
    tasks/userinfotask.cpp \
    tasks/icquserinfotask.cpp \
    tasks/usersearchtask.cpp \
    tasks/warningtask.cpp \
    tasks/typingnotifytask.cpp \
    tasks/buddyicontask.cpp \
    tasks/serverredirecttask.cpp \
    tasks/filetransfertask.cpp \
    tasks/chatnavservicetask.cpp \
    tasks/chatservicetask.cpp \
    tasks/icquserinfoupdatetask.cpp \
    tasks/icqmetawritetask.cpp \
    tasks/icqchangepasswordtask.cpp \
    tasks/icqtlvinforequesttask.cpp \
    tasks/icqtlvinfoupdatetask.cpp \
    tasks/chatroomtask.cpp \
    utils/rtf.cc \
    compat/compat.cpp

HEADERS += \
    buffer.h \
    chatroomhandler.h \
    client.h \
    connection.h \
    connectionhandler.h \
    contact.h \
    contactmanager.h \
    coreprotocol.h \
    filetransferhandler.h \
    flapprotocol.h \
    icqinfovalue.h \
    icquserinfo.h \
    inputprotocolbase.h \
    liboscar_export.h \
    oftmetatransfer.h \
    oftprotocol.h \
    ofttransfer.h \
    oscarclientstream.h \
    oscardebug.h \
    oscarguid.h \
    oscarmessage.h \
    oscarmessageplugin.h \
    oscarsettings.h \
    oscartypeclasses.h \
    oscartypes.h \
    oscarutils.h \
    rateclass.h \
    rateclassmanager.h \
    safedelete.h \
    snacprotocol.h \
    stream.h \
    tasks/blmlimitstask.h \
    tasks/buddyicontask.h \
    tasks/chatnavservicetask.h \
    tasks/chatroomtask.h \
    tasks/chatservicetask.h \
    tasks/clientreadytask.h \
    tasks/closeconnectiontask.h \
    tasks/errortask.h \
    tasks/filetransfertask.h \
    tasks/icbmparamstask.h \
    tasks/icqchangepasswordtask.h \
    tasks/icqtask.h \
    tasks/icqtlvinforequesttask.h \
    tasks/icqtlvinfoupdatetask.h \
    tasks/icquserinfotask.h \
    tasks/icquserinfoupdatetask.h \
    tasks/icqmetawritetask.h \
    tasks/locationrightstask.h \
    tasks/logintask.h \
    tasks/messageacktask.h \
    tasks/messagereceivertask.h \
    tasks/offlinemessagestask.h \
    tasks/icqofflinemessagestask.h \
    tasks/onlinenotifiertask.h \
    tasks/oscarlogintask.h \
    tasks/ownuserinfotask.h \
    tasks/prmparamstask.h \
    tasks/profiletask.h \
    tasks/rateinfotask.h \
    tasks/senddcinfotask.h \
    tasks/sendidletimetask.h \
    tasks/sendmessagetask.h \
    tasks/serverredirecttask.h \
    tasks/serverversionstask.h \
    tasks/servicesetuptask.h \
    tasks/ssiactivatetask.h \
    tasks/ssiauthtask.h \
    tasks/ssilisttask.h \
    tasks/ssimodifytask.h \
    tasks/ssiparamstask.h \
    tasks/task.h \
    tasks/typingnotifytask.h \
    tasks/userinfotask.h \
    tasks/usersearchtask.h \
    tasks/warningtask.h \
    transfer.h \
    userdetails.h \
    utils/rtf2html.h \
    xtrazxawayservice.h \
    xtrazxrandomizerservice.h \
    xtrazxservice.h \
    xtrazxtraznotify.h \
    compat/kdebug.h \
    compat/kdemacros.h \
    compat/oscar_export.h \
    compat/kio/global.h \
    compat/klocalizedstring.h \
    compat/krandom.h \
    compat/ksocketfactory.h

DISTFILES += COPYING COPYING.LIB COPYING.kopete DESIGN HACKING TODO README.md utils/rtf.ll

# In the app's RPM the library is private to the app (Harbour rule).
sailfishapp_build: LIBDIR = /usr/share/harbour-icqnina/lib
isEmpty(LIBDIR): LIBDIR = /usr/lib
target.path = $$LIBDIR
INSTALLS += target
