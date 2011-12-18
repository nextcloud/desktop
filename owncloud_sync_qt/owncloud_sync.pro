#-------------------------------------------------
#
# Project created by QtCreator 2011-10-30T17:04:10
#
#-------------------------------------------------

QT       += core gui network xml sql

unix:!macx:!symbian: {
    QT += dbus
    HEADERS +=
    SOURCES +=
    INCLUDEPATH += /usr/include/kde4
}

TARGET = owncloud_sync_qt
TEMPLATE = app


SOURCES += main.cpp\
        sqlite3_util.cpp \
        SyncWindow.cpp \
    qwebdav/QWebDAV.cpp \
    OwnCloudSync.cpp \
    OwnPasswordManager.cpp

HEADERS  += sqlite3_util.h \
            SyncWindow.h \
            qwebdav/QWebDAV.h \
    OwnCloudSync.h \
    SyncGlobal.h \
    OwnPasswordManager.h

FORMS    += SyncWindow.ui
INCLUDEPATH += qwebdav/

#INCLUDEPATH += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite
#SOURCES += $$[QT_INSTALL_PREFIX]/src/3rdparty/sqlite/sqlite3.c

RESOURCES += \
    owncloud_sync.qrc


unix:!symbian:!maemo5:isEmpty(MEEGO_VERSION_MAJOR) {
    target.path = /usr/bin
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

BITSIZE = $$system(getconf LONG_BIT)
if (contains(BITSIZE, 64)) {
#message($$BITSIZE)
unix:!macx:!symbian: LIBS += -L/usr/lib64/ -lsqlite3
unix:!macx:!symbian: LIBS += -L/usr/lib64/kde4/devel -lkparts -lkdeui -lkdecore
}
if (contains(BITSIZE, 32)) {
#message($$BITSIZE)
unix:!macx:!symbian: LIBS += -L/usr/lib/ -lsqlite3
unix:!macx:!symbian: LIBS += -L/usr/lib/kde4/devel -lkparts -lkdeui -lkdecore
}

#linux-g++ {
#message(On Linux)
#}
#macx-g++ {
#message(On Mac)
#}


INCLUDEPATH += /usr/include
DEPENDPATH += /usr/include

OTHER_FILES += \
    COPYING-README \
    COPYING-GPL










