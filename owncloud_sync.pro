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
