# MedChat Server - Linux 独立服务端
# 仅依赖 Qt5Core + Qt5Network + Qt5Sql

QT += core network sql gui
QT -= widgets

CONFIG += c++14 console
CONFIG -= app_bundle

TARGET = medchat-server
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/..

SOURCES += \
    main_server.cpp \
    ../common/protocol.cpp \
    ../server/chatserver.cpp \
    ../server/clienthandler.cpp \
    ../server/serverdb.cpp \
    ../server/usermanager.cpp

HEADERS += \
    ../common/protocol.h \
    ../common/constants.h \
    ../server/chatserver.h \
    ../server/clienthandler.h \
    ../server/serverdb.h \
    ../server/usermanager.h
