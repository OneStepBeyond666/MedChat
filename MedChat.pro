QT += core gui network widgets

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
    server/usermanager.cpp \
    client/chatclient.cpp \
    client/chathistory.cpp \
    client/loginwindow.cpp \
    client/mainwindow.cpp \
    ui/contactlistwidget.cpp \
    ui/chatwidget.cpp \
    ui/messagebubble.cpp

HEADERS += \
    common/protocol.h \
    server/chatserver.h \
    server/clienthandler.h \
    server/usermanager.h \
    client/chatclient.h \
    client/chathistory.h \
    client/loginwindow.h \
    client/mainwindow.h \
    ui/contactlistwidget.h \
    ui/chatwidget.h \
    ui/messagebubble.h

RESOURCES += \
    resources/style.qrc
