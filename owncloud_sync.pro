#-------------------------------------------------
#
# Project created by QtCreator 2011-10-30T17:04:10
#
#-------------------------------------------------

QT       += core gui network xml sql

TARGET = owncloud_sync
TEMPLATE = app


SOURCES += main.cpp\
        SyncWindow.cpp \
    qwebdav/QWebDAV.cpp

HEADERS  += SyncWindow.h \
    qwebdav/QWebDAV.h

FORMS    += SyncWindow.ui
INCLUDEPATH += qwebdav/

RESOURCES += \
    owncloud_sync.qrc

