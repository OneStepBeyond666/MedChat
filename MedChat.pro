QT += core gui network widgets sql

CONFIG += c++14 console
TARGET = MedChat
TEMPLATE = app

# 服务端和客户端代码都编译在一起，运行时通过命令行参数区分
DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    common/protocol.cpp \
    server/chatserver.cpp \
    server/clienthandler.cpp \
    server/serverdb.cpp \
    server/usermanager.cpp \
    client/chatclient.cpp \
    client/localdb.cpp \
    client/loginwindow.cpp \
    client/mainwindow.cpp \
    ui/addfrienddialog.cpp \
    ui/avatarcropper.cpp \
    ui/contactlistwidget.cpp \
    ui/chatwidget.cpp \
    ui/leftsidebar.cpp \
    ui/messagebubble.cpp \
    ui/profiledialog.cpp \
    ui/sessionlistwidget.cpp

HEADERS += \
    common/protocol.h \
    common/constants.h \
    server/chatserver.h \
    server/clienthandler.h \
    server/serverdb.h \
    server/usermanager.h \
    client/chatclient.h \
    client/localdb.h \
    client/loginwindow.h \
    client/mainwindow.h \
    ui/addfrienddialog.h \
    ui/avatarcropper.h \
    ui/contactlistwidget.h \
    ui/chatwidget.h \
    ui/leftsidebar.h \
    ui/messagebubble.h \
    ui/profiledialog.h \
    ui/sessionlistwidget.h

RESOURCES += \
    resources/style.qrc
