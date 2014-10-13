/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "mirall/socketapi.h"

#include "mirall/mirallconfigfile.h"
#include "mirall/folderman.h"
#include "mirall/folder.h"
#include "mirall/utility.h"
#include "mirall/theme.h"
#include "mirall/syncjournalfilerecord.h"
#include "mirall/syncfileitem.h"
#include "mirall/filesystem.h"
#include "version.h"

#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QStringList>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QLocalSocket>

#include <sqlite3.h>


#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QStandardPaths>
#endif


// This is the version that is returned when the client asks for the VERSION.
// The first number should be changed if there is an incompatible change that breaks old clients.
// The second number should be changed when there are new features.
#define MIRALL_SOCKET_API_VERSION "1.0"

extern "C" {

enum csync_exclude_type_e {
  CSYNC_NOT_EXCLUDED   = 0,
  CSYNC_FILE_SILENTLY_EXCLUDED,
  CSYNC_FILE_EXCLUDE_AND_REMOVE,
  CSYNC_FILE_EXCLUDE_LIST,
  CSYNC_FILE_EXCLUDE_INVALID_CHAR
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

CSYNC_EXCLUDE_TYPE csync_excluded_no_ctx(c_strlist_t *excludes, const char *path, int filetype);
int csync_exclude_load(const char *fname, c_strlist_t **list);
}

namespace Mirall {

#define DEBUG qDebug() << "SocketApi: "

namespace SocketApiHelper {

SyncFileStatus fileStatus(Folder *folder, const QString& systemFileName, c_strlist_t *excludes );

SyncJournalFileRecord dbFileRecord_capi( Folder *folder, QString fileName )
{

    // FIXME: Check if this stat is really needed, or is it done in the caller?
    if( !(folder && folder->journalDb()) ) {
        return SyncJournalFileRecord();
    }

    QFileInfo fi(fileName);
    if( fi.isAbsolute() ) {
        fileName.remove(0, folder->path().length());
    }

    QString dbFileName = folder->journalDb()->databaseFilePath();

    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;
    SyncJournalFileRecord rec;
    int rc;
    const char* query = "SELECT inode, mode, modtime, type, md5, fileid, remotePerm FROM "
            "metadata WHERE phash=:ph";

    if( sqlite3_open_v2(dbFileName.toUtf8().constData(), &db,
                        SQLITE_OPEN_READONLY+SQLITE_OPEN_NOMUTEX, NULL) == SQLITE_OK ) {

        rc = sqlite3_prepare_v2(db, query, strlen(query), &stmt, NULL);
        if( rc != SQLITE_OK ) {
            qDebug() << "Unable to prepare the query statement.";
            return rec;
        }
        qlonglong phash = SyncJournalDb::getPHash( fileName );
        sqlite3_bind_int64(stmt, 1, (long long signed int)phash);

        // int column_count = sqlite3_column_count(stmt);

        rc = sqlite3_step(stmt);

        if (rc == SQLITE_ROW ) {
            rec._path   = fileName;
            rec._inode  = sqlite3_column_int64(stmt,0);;
            rec._mode = sqlite3_column_int(stmt, 1);
            rec._modtime = Utility::qDateTimeFromTime_t( strtoul((char*)sqlite3_column_text(stmt, 2), NULL, 10));
            rec._type = sqlite3_column_int(stmt, 3);;
            rec._etag = QByteArray((char*)sqlite3_column_text(stmt, 4));
            rec._fileId = QByteArray((char*)sqlite3_column_text(stmt, 5));
            rec._remotePerm = QByteArray((char*)sqlite3_column_text(stmt, 6));
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }
    return rec;
}

SyncJournalFileRecord dbFileRecord( Folder *folder, QString fileName )
{
    if( !folder ) {
        return SyncJournalFileRecord();
    }

    QFileInfo fi(fileName);
    if( fi.isAbsolute() ) {
        fileName.remove(0, folder->path().length());
    }
    return( folder->journalDb()->getFileRecord(fileName) );
}

/**
 * Get status about a single file.
 */
SyncFileStatus fileStatus(Folder *folder, const QString& systemFileName, c_strlist_t *excludes )
{
    QString file = folder->path();
    QString fileName = systemFileName.normalized(QString::NormalizationForm_C);

    bool isSyncRootFolder = true;
    if( fileName != QLatin1String("/") && !fileName.isEmpty() ) {
        file = folder->path() + fileName;
        isSyncRootFolder = false;
    }

    if( fileName.endsWith(QLatin1Char('/')) ) {
        fileName.truncate(fileName.length()-1);
        qDebug() << "Removed trailing slash: " << fileName;
    }

    QFileInfo fi(file);

    if( !fi.exists() ) {
        qDebug() << "OO File " << file << " is not existing";
        return SyncFileStatus(SyncFileStatus::STATUS_STAT_ERROR);
    }

    // file is ignored?
    if( fi.isSymLink() ) {
        return SyncFileStatus(SyncFileStatus::STATUS_IGNORE);
    }

    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    // '\' is ignored, so convert to unix path before passing the path in.
    QString unixFileName = QDir::fromNativeSeparators(fileName);

    CSYNC_EXCLUDE_TYPE excl = csync_excluded_no_ctx(excludes, unixFileName.toUtf8(), type);
    if( excl != CSYNC_NOT_EXCLUDED ) {
        return SyncFileStatus(SyncFileStatus::STATUS_IGNORE);
    }


    SyncFileStatus status(SyncFileStatus::STATUS_NONE);
    if (type == CSYNC_FTW_TYPE_DIR) {
        if (folder->estimateState(fileName, type, &status)) {
            qDebug() << Q_FUNC_INFO << "Folder estimated status for" << fileName << "to" << status.toSocketAPIString();
            return status;
        }
        if (fileName == "") {
            // sync folder itself
            if (folder->syncResult().status() == SyncResult::Undefined
                    || folder->syncResult().status() == SyncResult::NotYetStarted
                    || folder->syncResult().status() == SyncResult::SyncPrepare
                    || folder->syncResult().status() == SyncResult::SyncRunning
                    || folder->syncResult().status() == SyncResult::Paused) {
                status.set(SyncFileStatus::STATUS_EVAL);
                return status;
            } else if (folder->syncResult().status() == SyncResult::Success
                       || folder->syncResult().status() == SyncResult::Problem) {
                status.set(SyncFileStatus::STATUS_SYNC);
                return status;
            }  else if (folder->syncResult().status() == SyncResult::Error
                        || folder->syncResult().status() == SyncResult::SetupError
                        || folder->syncResult().status() == SyncResult::SyncAbortRequested) {
                status.set(SyncFileStatus::STATUS_ERROR);
                return status;
            }
        }
        SyncJournalFileRecord rec = dbFileRecord_capi(folder, unixFileName );
        if (rec.isValid()) {
            status.set(SyncFileStatus::STATUS_SYNC);
            if (rec._remotePerm.contains("S")) {
               status.setSharedWithMe(true);
            }
        } else {
            status.set(SyncFileStatus::STATUS_EVAL);
        }
    } else if (type == CSYNC_FTW_TYPE_FILE) {
        if (folder->estimateState(fileName, type, &status)) {
            return status;
        }
        SyncJournalFileRecord rec = dbFileRecord_capi(folder, unixFileName );
        if (rec.isValid()) {
            if (rec._remotePerm.contains("S")) {
               status.setSharedWithMe(true);
            }
            if( FileSystem::getModTime(fi.absoluteFilePath()) == Utility::qDateTimeToTime_t(rec._modtime) ) {
                status.set(SyncFileStatus::STATUS_SYNC);
                return status;
            } else {
                status.set(SyncFileStatus::STATUS_EVAL);
                return status;
            }
        }
        status.set(SyncFileStatus::STATUS_NEW);
        return status;
    }

    return status;
}

} // namespace

SocketApi::SocketApi(QObject* parent)
    : QObject(parent)
    , _excludes(0)
{
    QString socketPath;

    if (Utility::isWindows()) {
        socketPath = QLatin1String("\\\\.\\pipe\\")
                + Theme::instance()->appName();
    } else if (Utility::isMac()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        // Always using Qt5 on OS X
        QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        socketPath = runtimeDir + "/SyncStateHelper/" + Theme::instance()->appName() + ".socket";
        // We use the generic SyncStateHelper name on OS X since the different branded clients
        // should unfortunately not mention that they are ownCloud :-)
#endif
    } else if( Utility::isLinux() ) {
        QString runtimeDir;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#else
        runtimeDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
#endif
        socketPath = runtimeDir + "/" + Theme::instance()->appName() + "/socket";
    } else {
	DEBUG << "An unexpected system detected";
    }

    QLocalServer::removeServer(socketPath);
    QFileInfo info(socketPath);
    if (!info.dir().exists()) {
        bool result = info.dir().mkpath(".");
        DEBUG << "creating" << info.dir().path() << result;
        if( result ) {
            QFile::setPermissions(socketPath,
                                  QFile::Permissions(QFile::ReadOwner+QFile::WriteOwner+QFile::ExeOwner));
        }
    }
    if(!_localServer.listen(socketPath)) {
        DEBUG << "can't start server" << socketPath;
    } else {
        DEBUG << "server started, listening at " << socketPath;
    }

    connect(&_localServer, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    // folder watcher
    connect(FolderMan::instance(), SIGNAL(folderSyncStateChange(QString)), this, SLOT(slotUpdateFolderView(QString)));
    connect(ProgressDispatcher::instance(), SIGNAL(jobCompleted(QString,SyncFileItem)),
            SLOT(slotJobCompleted(QString,SyncFileItem)));
    connect(ProgressDispatcher::instance(), SIGNAL(syncItemDiscovered(QString,SyncFileItem)),
            this, SLOT(slotSyncItemDiscovered(QString,SyncFileItem)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer.close();
    slotClearExcludesList();
}

void SocketApi::slotClearExcludesList()
{
    c_strlist_clear(_excludes);
}

void SocketApi::slotReadExcludes()
{
    MirallConfigFile cfgFile;
    slotClearExcludesList();
    QString excludeList = cfgFile.excludeFile( MirallConfigFile::SystemScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added system ignore list to socketapi:" << excludeList.toUtf8();
        csync_exclude_load(excludeList.toUtf8(), &_excludes);
    }
    excludeList = cfgFile.excludeFile( MirallConfigFile::UserScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added user defined ignore list to csync:" << excludeList.toUtf8();
        csync_exclude_load(excludeList.toUtf8(), &_excludes);
    }
}

void SocketApi::slotNewConnection()
{
    SocketType* socket = _localServer.nextPendingConnection();

    if( ! socket ) {
        return;
    }
    DEBUG << "New connection" << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(slotReadSocket()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    Q_ASSERT(socket->readAll().isEmpty());

    _listeners.append(socket);

#ifdef Q_OS_MAC
    // We want to tell our location so it can load the icons
    // e.g. "/Users/guruz/woboq/owncloud/client/buildmirall/owncloud.app/Contents/MacOS/"
    QString iconPath = qApp->applicationDirPath() + "/../Resources/icons/";
    if (!QDir(iconPath).exists()) {
        DEBUG << "Icon path " << iconPath << " does not exist, did you forget make install?";
    }
    broadcastMessage(QLatin1String("ICON_PATH"), iconPath );
#endif


    foreach( QString alias, FolderMan::instance()->map().keys() ) {
       slotRegisterPath(alias);
    }
}

void SocketApi::onLostConnection()
{
    DEBUG << "Lost connection " << sender();

    SocketType* socket = qobject_cast<SocketType*>(sender());
    _listeners.removeAll(socket);
}


void SocketApi::slotReadSocket()
{
    SocketType* socket = qobject_cast<SocketType*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,SocketType*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1) {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(SocketType*, socket));
        } else {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::slotRegisterPath( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        broadcastMessage(QLatin1String("REGISTER_PATH"), f->path() );
    }
}

void SocketApi::slotUnregisterPath( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        broadcastMessage(QLatin1String("UNREGISTER_PATH"), f->path(), QString::null, true );
    }
}

void SocketApi::slotUpdateFolderView(const QString& alias)
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        // do only send UPDATE_VIEW for a couple of status
        if( f->syncResult().status() == SyncResult::SyncPrepare ||
                f->syncResult().status() == SyncResult::Success ||
                f->syncResult().status() == SyncResult::Paused  ||
                f->syncResult().status() == SyncResult::Problem ||
                f->syncResult().status() == SyncResult::Error   ||
                f->syncResult().status() == SyncResult::SetupError ) {
            if( Utility::isWindows() ) {
                Utility::winShellChangeNotify( f->path() );
            } else {
                broadcastMessage(QLatin1String("UPDATE_VIEW"), f->path() );
            }
        } else {
            qDebug() << "Not sending UPDATE_VIEW for" << alias << "because status() is" << f->syncResult().status();
        }
    }
}

void SocketApi::slotJobCompleted(const QString &folder, const SyncFileItem &item)
{
    Folder *f = FolderMan::instance()->folder(folder);
    if (!f) {
        return;
    }

    const QString path = f->path() + item.destination();

    QString command = QLatin1String("OK");
    if (Progress::isWarningKind(item._status)) {
        command = QLatin1String("ERROR");
    }
    broadcastMessage(QLatin1String("STATUS"), path, command);
}

void SocketApi::slotSyncItemDiscovered(const QString &folder, const SyncFileItem &item)
{
    if (item._instruction == CSYNC_INSTRUCTION_NONE) {
        return;
    }

    Folder *f = FolderMan::instance()->folder(folder);
    if (!f) {
        return;
    }

    const QString path = f->path() + item.destination();

    const QString command = QLatin1String("SYNC");
    broadcastMessage(QLatin1String("STATUS"), path, command);
}



void SocketApi::sendMessage(SocketType *socket, const QString& message, bool doWait)
{
    DEBUG << "Sending message: " << message;
    QString localMessage = message;
    if( ! localMessage.endsWith(QLatin1Char('\n'))) {
        localMessage.append(QLatin1Char('\n'));
    }

    QByteArray bytesToSend = localMessage.toUtf8();
    qint64 sent = socket->write(bytesToSend);
    if( doWait ) {
        socket->waitForBytesWritten(1000);
    }
    if( sent != bytesToSend.length() ) {
        qDebug() << "WARN: Could not send all data on socket for " << localMessage;
    }

}

void SocketApi::broadcastMessage( const QString& verb, const QString& path, const QString& status, bool doWait )
{
    QString msg(verb);

    if( !status.isEmpty() ) {
        msg.append(QLatin1Char(':'));
        msg.append(status);
    }
    if( !path.isEmpty() ) {
        msg.append(QLatin1Char(':'));
        msg.append(QDir::toNativeSeparators(path));
    }

    // sendMessage already has a debug output
    //DEBUG << "Broadcasting to" << _listeners.count() << "listeners: " << msg;
    foreach(SocketType *socket, _listeners) {
        sendMessage(socket, msg, doWait);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString& argument, SocketType* socket)
{
    // This command is the same as RETRIEVE_FILE_STATUS

    qDebug() << Q_FUNC_INFO << argument;
    command_RETRIEVE_FILE_STATUS(argument, socket);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString& argument, SocketType* socket)
{
    if( !socket ) {
        qDebug() << "No valid socket object.";
        return;
    }

    qDebug() << Q_FUNC_INFO << argument;

    QString statusString;

    Folder* syncFolder = FolderMan::instance()->folderForPath( argument );
    if (!syncFolder) {
        // this can happen in offline mode e.g.: nothing to worry about
        DEBUG << "folder offline or not watched:" << argument;
        statusString = QLatin1String("NOP");
    } else {
        const QString file = argument.mid(syncFolder->path().length());
        SyncFileStatus fileStatus = SocketApiHelper::fileStatus(syncFolder, file, _excludes);

        statusString = fileStatus.toSocketAPIString();
    }

    QString message = QLatin1String("STATUS:")+statusString+QLatin1Char(':')
            +QDir::toNativeSeparators(argument);
    sendMessage(socket, message);
}

void SocketApi::command_VERSION(const QString&, SocketType* socket)
{
    sendMessage(socket, QLatin1String("VERSION:" MIRALL_VERSION_STRING ":" MIRALL_SOCKET_API_VERSION));
}


} // namespace Mirall

