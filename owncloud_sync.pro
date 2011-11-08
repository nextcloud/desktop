#-------------------------------------------------
#
# Project created by QtCreator 2011-10-30T17:04:10
#
#-------------------------------------------------

QT       += core gui network xml sql

TARGET = owncloud_sync
TEMPLATE = app


SOURCES += main.cpp\
        sqlite3_util.cpp \
        SyncWindow.cpp \
    qwebdav/QWebDAV.cpp \
    OwnCloudSync.cpp

HEADERS  += sqlite3_util.h \
            SyncWindow.h \
            qwebdav/QWebDAV.h \
    OwnCloudSync.h

FORMS    += SyncWindow.ui
INCLUDEPATH += qwebdav/

#INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite
#SOURCES += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/sqlite3.c

RESOURCES += \
    owncloud_sync.qrc


unix:!symbian:!maemo5:isEmpty(MEEGO_VERSION_MAJOR) {
    target.path = /opt/owncloud_sync/bin
    INSTALLS += target
}

unix:!symbian:!maemo5:isEmpty(MEEGO_VERSION_MAJOR) {
    desktopfile.files = $${TARGET}.desktop
    desktopfile.path = /usr/share/applications
    INSTALLS += desktopfile
}

unix:!symbian:!maemo5:isEmpty(MEEGO_VERSION_MAJOR) {
    icon.files = owncloud_sync.png
    icon.path = /usr/share/icons/hicolor/64x64/apps
    INSTALLS += icon
}



unix:!macx:!symbian: LIBS += -L$$PWD/../../../../../usr/lib64/ -lsqlite3

INCLUDEPATH += $$PWD/../../../../../usr/include
DEPENDPATH += $$PWD/../../../../../usr/include


