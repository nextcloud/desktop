/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#include "socketapi.h"

#include "config.h"
#include "configfile.h"
#include "folderman.h"
#include "folder.h"
#include "utility.h"
#include "theme.h"
#include "syncjournalfilerecord.h"
#include "syncfileitem.h"
#include "filesystem.h"
#include "version.h"
#include "accountstate.h"

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
  CSYNC_FILE_EXCLUDE_INVALID_CHAR,
  CSYNC_FILE_EXCLUDE_LONG_FILENAME
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

CSYNC_EXCLUDE_TYPE csync_excluded_no_ctx(c_strlist_t *excludes, const char *path, int filetype);
int csync_exclude_load(const char *fname, c_strlist_t **list);
}

namespace OCC {

#define DEBUG qDebug() << "SocketApi: "

SocketApi::SocketApi(QObject* parent)
    : QObject(parent)
    , _excludes(0)
{
    QString socketPath;

    if (Utility::isWindows()) {
        socketPath = QLatin1String("\\\\.\\pipe\\")
        + QLatin1String("ownCloud");
        // TODO: once the windows extension supports multiple
        // client connections, switch back to the theme name
        // See issue #2388
        // + Theme::instance()->appName();
    } else if (Utility::isMac()) {
        // This must match the code signing Team setting of the extension
        // Example for developer builds (with ad-hoc signing identity): "" "com.owncloud.desktopclient" ".socketApi"
        // Example for official signed packages: "9B5WD74GWJ." "com.owncloud.desktopclient" ".socketApi"
        socketPath = SOCKETAPI_TEAM_IDENTIFIER_PREFIX APPLICATION_REV_DOMAIN ".socketApi";
    } else if( Utility::isLinux() || Utility::isBSD() ) {
        QString runtimeDir;
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
#else
        runtimeDir = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
        if (runtimeDir.isEmpty()) {
            runtimeDir = QDir::tempPath() + QLatin1String("/runtime-")
                + QString::fromLocal8Bit(qgetenv("USER"));
            QDir().mkdir(runtimeDir);
        }
#endif
        socketPath = runtimeDir + "/" + Theme::instance()->appName() + "/socket";
    } else {
	DEBUG << "An unexpected system detected";
    }

    SocketApiServer::removeServer(socketPath);
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
    connect(FolderMan::instance(), SIGNAL(folderSyncStateChange(Folder*)), this, SLOT(slotUpdateFolderView(Folder*)));
    connect(ProgressDispatcher::instance(), SIGNAL(jobCompleted(QString, const SyncFileItem &)),
            SLOT(slotJobCompleted(QString, const SyncFileItem &)));
    connect(ProgressDispatcher::instance(), SIGNAL(syncItemDiscovered(QString, const SyncFileItem &)),
            this, SLOT(slotSyncItemDiscovered(QString, const SyncFileItem &)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer.close();
    // All remaining sockets will be destroyed with _localServer, their parent
    Q_ASSERT(_listeners.isEmpty() || _listeners.first()->parent() == &_localServer);
    _listeners.clear();
    slotClearExcludesList();
}

void SocketApi::slotClearExcludesList()
{
    c_strlist_clear(_excludes);
}

void SocketApi::slotReadExcludes()
{
    ConfigFile cfgFile;
    slotClearExcludesList();
    QString excludeList = cfgFile.excludeFile( ConfigFile::SystemScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added system ignore list to socketapi:" << excludeList.toUtf8();
        csync_exclude_load(excludeList.toUtf8(), &_excludes);
    }
    excludeList = cfgFile.excludeFile( ConfigFile::UserScope );
    if( !excludeList.isEmpty() ) {
        qDebug() << "==== added user defined ignore list to csync:" << excludeList.toUtf8();
        csync_exclude_load(excludeList.toUtf8(), &_excludes);
    }
}

void SocketApi::slotNewConnection()
{
    QIODevice* socket = _localServer.nextPendingConnection();

    if( ! socket ) {
        return;
    }
    DEBUG << "New connection" << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(slotReadSocket()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    Q_ASSERT(socket->readAll().isEmpty());

    _listeners.append(socket);

    foreach( Folder *f, FolderMan::instance()->map() ) {
        QString message = buildRegisterPathMessage(f->path());
        sendMessage(socket, message);
    }
}

void SocketApi::onLostConnection()
{
    DEBUG << "Lost connection " << sender();

    QIODevice* socket = qobject_cast<QIODevice*>(sender());
    _listeners.removeAll(socket);
    socket->deleteLater();
}


void SocketApi::slotReadSocket()
{
    QIODevice* socket = qobject_cast<QIODevice*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,QIODevice*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1) {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(QIODevice*, socket));
        } else {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::slotRegisterPath( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        QString message = buildRegisterPathMessage(f->path());
        foreach(QIODevice *socket, _listeners) {
            sendMessage(socket, message);
        }
    }
}

void SocketApi::slotUnregisterPath( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        broadcastMessage(QLatin1String("UNREGISTER_PATH"), f->path(), QString::null, true );

        if( _dbQueries.contains(f)) {
            auto h = _dbQueries[f];
            if( h ) {
                h->finish();
            }
            _dbQueries.remove(f);
        }
        if( _openDbs.contains(f) ) {
            auto db = _openDbs[f];
            if( db ) {
                db->close();
            }
            _openDbs.remove(f);
        }
    }
}

void SocketApi::slotUpdateFolderView(Folder *f)
{
    if (_listeners.isEmpty()) {
        return;
    }

    if (f) {
        // do only send UPDATE_VIEW for a couple of status
        if( f->syncResult().status() == SyncResult::SyncPrepare ||
                f->syncResult().status() == SyncResult::Success ||
                f->syncResult().status() == SyncResult::Paused  ||
                f->syncResult().status() == SyncResult::Problem ||
                f->syncResult().status() == SyncResult::Error   ||
                f->syncResult().status() == SyncResult::SetupError ) {

            broadcastMessage(QLatin1String("STATUS"), f->path() ,
                             this->fileStatus(f, "", _excludes).toSocketAPIString());

            broadcastMessage(QLatin1String("UPDATE_VIEW"), f->path() );
        } else {
            qDebug() << "Not sending UPDATE_VIEW for" << f->alias() << "because status() is" << f->syncResult().status();
        }
    }
}

void SocketApi::slotJobCompleted(const QString &folder, const SyncFileItem &item)
{
    if (_listeners.isEmpty()) {
        return;
    }

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
    if (_listeners.isEmpty()) {
        return;
    }

    if (item._instruction == CSYNC_INSTRUCTION_NONE) {
        return;
    }

    Folder *f = FolderMan::instance()->folder(folder);
    if (!f) {
        return;
    }

    QString path = f->path() + item.destination();

    // the trailing slash for directories must be appended as the filenames coming in
    // from the plugins have that too. Otherwise the according entry item is not found
    // in the plugin.
    if( item._type == SyncFileItem::Type::Directory ) {
        path += QLatin1Char('/');
    }

    const QString command = QLatin1String("SYNC");
    broadcastMessage(QLatin1String("STATUS"), path, command);
}



void SocketApi::sendMessage(QIODevice *socket, const QString& message, bool doWait)
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
        QFileInfo fi(path);
        msg.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    }

    foreach(QIODevice *socket, _listeners) {
        sendMessage(socket, msg, doWait);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString& argument, QIODevice* socket)
{
    // This command is the same as RETRIEVE_FILE_STATUS

    //qDebug() << Q_FUNC_INFO << argument;
    command_RETRIEVE_FILE_STATUS(argument, socket);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString& argument, QIODevice* socket)
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
        const QString file = QDir::cleanPath(argument).mid(syncFolder->cleanPath().length()+1);
        SyncFileStatus fileStatus = this->fileStatus(syncFolder, file, _excludes);

        statusString = fileStatus.toSocketAPIString();
    }

    QString message = QLatin1String("STATUS:")+statusString+QLatin1Char(':')
            +QDir::toNativeSeparators(argument);
    sendMessage(socket, message);
}

void SocketApi::command_SHARE(const QString& localFile, QIODevice* socket)
{
    if (!socket) {
        qDebug() << Q_FUNC_INFO << "No valid socket object.";
        return;
    }

    qDebug() << Q_FUNC_INFO << localFile;

    Folder *shareFolder = FolderMan::instance()->folderForPath(localFile);
    if (!shareFolder) {
        const QString message = QLatin1String("SHARE:NOP:")+QDir::toNativeSeparators(localFile);
        // files that are not within a sync folder are not synced.
        sendMessage(socket, message);
    } else if (!shareFolder->accountState()->isConnected()) {
        const QString message = QLatin1String("SHARE:NOTCONNECTED:")+QDir::toNativeSeparators(localFile);
        // if the folder isn't connected, don't open the share dialog
        sendMessage(socket, message);
    } else {
        const QString folderForPath = shareFolder->path();
        const QString remotePath = shareFolder->remotePath() + localFile.right(localFile.count()-folderForPath.count()+1);

        SyncJournalFileRecord rec = dbFileRecord_capi(shareFolder, localFile);

        bool allowReshare = true; // lets assume the good
        if( rec.isValid() ) {
            // check the permission: Is resharing allowed?
            if( !rec._remotePerm.contains('R') ) {
                allowReshare = false;
            }
        }
        const QString message = QLatin1String("SHARE:OK:")+QDir::toNativeSeparators(localFile);
        sendMessage(socket, message);

        emit shareCommandReceived(remotePath, localFile, allowReshare);
    }
}

void SocketApi::command_VERSION(const QString&, QIODevice* socket)
{
    sendMessage(socket, QLatin1String("VERSION:" MIRALL_VERSION_STRING ":" MIRALL_SOCKET_API_VERSION));
}

void SocketApi::command_SHARE_MENU_TITLE(const QString &, QIODevice* socket)
{
    sendMessage(socket, QLatin1String("SHARE_MENU_TITLE:") + tr("Share with %1", "parameter is ownCloud").arg(Theme::instance()->appNameGUI()));
}

QString SocketApi::buildRegisterPathMessage(const QString& path)
{
    QFileInfo fi(path);
    QString message = QLatin1String("REGISTER_PATH:");
    message.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    return message;
}

SqlQuery* SocketApi::getSqlQuery( Folder *folder )
{
    if( !folder ) {
        return 0;
    }

    if( _dbQueries.contains(folder) ) {
        return _dbQueries[folder].data();
    }

    /* No valid sql query object yet for this folder */
    int rc;
    const QString sql("SELECT inode, mode, modtime, type, md5, fileid, remotePerm FROM "
                      "metadata WHERE phash=?1");
    QString dbFileName = folder->journalDb()->databaseFilePath();

    QFileInfo fi(dbFileName);
    if( fi.exists() ) {
        auto db = QSharedPointer<SqlDatabase>::create();

        if( db && db->openReadOnly(dbFileName) ) {
            _openDbs.insert(folder, db);

            QSharedPointer<SqlQuery> query(new SqlQuery(*db));
            rc = query->prepare(sql);

            if( rc != SQLITE_OK ) {
                qDebug() << "Unable to prepare the query statement:" << rc;
                return 0; // do not insert into hash
            }
            _dbQueries.insert( folder, query);
            return query.data();
        } else {
            qDebug() << "Unable to open db" << dbFileName;
        }
    } else {
        qDebug() << Q_FUNC_INFO << "Journal to query does not yet exist.";
    }
    return 0;
}

SyncJournalFileRecord SocketApi::dbFileRecord_capi( Folder *folder, QString fileName )
{
    if( !(folder && folder->journalDb()) ) {
        return SyncJournalFileRecord();
    }

    if( fileName.startsWith( folder->path() )) {
        fileName.remove(0, folder->path().length());
    }

    // remove trailing slash
    if( fileName.endsWith( QLatin1Char('/') ) ) {
        fileName.truncate(fileName.length()-1);
    }
    SqlQuery *query = getSqlQuery(folder);
    SyncJournalFileRecord rec;

    if( query ) {
        qlonglong phash = SyncJournalDb::getPHash( fileName );
        query->bindValue(1, phash);
        // int column_count = sqlite3_column_count(stmt);

        if (query->next()) {
            rec._path    = fileName;
            rec._inode   = query->int64Value(0);
            rec._mode    = query->intValue(1);
            rec._modtime = Utility::qDateTimeFromTime_t( query->int64Value(2));
            rec._type    = query->intValue(3);
            rec._etag    = query->baValue(4);
            rec._fileId  = query->baValue(5);
            rec._remotePerm = query->baValue(6);
        }
        query->reset();
    }
    return rec;
}

/**
 * Get status about a single file.
 */
SyncFileStatus SocketApi::fileStatus(Folder *folder, const QString& systemFileName, c_strlist_t *excludes )
{
    QString file = folder->path();
    QString fileName = systemFileName.normalized(QString::NormalizationForm_C);
    QString fileNameSlash = fileName;

    if( fileName != QLatin1String("/") && !fileName.isEmpty() ) {
        file = folder->path() + fileName;
    }

    if( fileName.endsWith(QLatin1Char('/')) ) {
        fileName.truncate(fileName.length()-1);
        qDebug() << "Removed trailing slash: " << fileName;
    } else {
        fileNameSlash += QLatin1Char('/');
    }

    if( !FileSystem::fileExists(file) ) {
        qDebug() << "OO File " << file << " is not existing";
        return SyncFileStatus(SyncFileStatus::STATUS_STAT_ERROR);
    }

    // file is ignored?
    // Qt considers .lnk files symlinks on Windows so we need to work
    // around that here.
    const QFileInfo fi(file);
    if( fi.isSymLink()
#ifdef Q_OS_WIN
            && fi.suffix() != "lnk"
#endif
            ) {
        return SyncFileStatus(SyncFileStatus::STATUS_IGNORE);
    }

    csync_ftw_type_e type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    // Is it excluded?
    CSYNC_EXCLUDE_TYPE excl = csync_excluded_no_ctx(excludes, fileName.toUtf8(), type);
    if( excl != CSYNC_NOT_EXCLUDED ) {
        return SyncFileStatus(SyncFileStatus::STATUS_IGNORE);
    }

    // Error if it is in the selective sync blacklistr
    foreach(const auto &s, folder->journalDb()->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList)) {
        if (fileNameSlash.startsWith(s)) {
            return SyncFileStatus(SyncFileStatus::STATUS_ERROR);
        }
    }

    SyncFileStatus status(SyncFileStatus::STATUS_NONE);
    SyncJournalFileRecord rec = dbFileRecord_capi(folder, fileName );

    if (folder->estimateState(fileName, type, &status)) {
        qDebug() << Q_FUNC_INFO << "Folder estimated status for" << fileName << "to" << status.toSocketAPIString();
    } else if (fileName == "") {
        // sync folder itself
        switch (folder->syncResult().status()) {
        case SyncResult::Undefined:
        case SyncResult::NotYetStarted:
        case SyncResult::SyncPrepare:
        case SyncResult::SyncRunning:
            status.set(SyncFileStatus::STATUS_EVAL);
            return status;

        case SyncResult::Success:
        case SyncResult::Problem:
            status.set(SyncFileStatus::STATUS_SYNC);
            return status;

        case SyncResult::Error:
        case SyncResult::SetupError:
        case SyncResult::SyncAbortRequested:
            status.set(SyncFileStatus::STATUS_ERROR);
            return status;

        case SyncResult::Paused:
            status.set(SyncFileStatus::STATUS_IGNORE);
            return status;
        }
    } else if (type == CSYNC_FTW_TYPE_DIR) {
        if (rec.isValid()) {
            status.set(SyncFileStatus::STATUS_SYNC);
        } else {
            qDebug() << Q_FUNC_INFO << "Could not determine state for folder" << fileName << "will set STATUS_NEW";
            status.set(SyncFileStatus::STATUS_NEW);
        }
    } else if (type == CSYNC_FTW_TYPE_FILE) {
        if (rec.isValid()) {
            if( FileSystem::getModTime(fi.absoluteFilePath()) == Utility::qDateTimeToTime_t(rec._modtime) ) {
                status.set(SyncFileStatus::STATUS_SYNC);
            } else {
                if (rec._remotePerm.isNull() || rec._remotePerm.contains("W") ) {
                    status.set(SyncFileStatus::STATUS_EVAL);
                } else {
                    status.set(SyncFileStatus::STATUS_ERROR);
                }
            }
        } else {
            qDebug() << Q_FUNC_INFO << "Could not determine state for file" << fileName << "will set STATUS_NEW";
            status.set(SyncFileStatus::STATUS_NEW);
        }
    }

    if (rec.isValid()) {
        if (rec._remotePerm.isNull()) {
            // probably owncloud 6, that does not have permissions flag yet.
            QString url = folder->remoteUrl().toString() + fileName;
            if (url.contains(QLatin1String("/remote.php/webdav/Shared/"))) {
                status.setSharedWithMe(true);
            }
        } else if (rec._remotePerm.contains("S")) {
            status.setSharedWithMe(true);
        }
    }
    if (status.tag() == SyncFileStatus::STATUS_NEW) {
        // check the parent folder if it is shared and if it is allowed to create a file/dir within
        QDir d( fi.path() );
        auto parentPath = d.path();
        auto dirRec = dbFileRecord_capi(folder, parentPath);
        bool isDir = type == CSYNC_FTW_TYPE_DIR;
        while( !d.isRoot() && !(d.exists() && dirRec.isValid()) ) {
            d.cdUp(); // returns true if the dir exists.

            parentPath = d.path();
            // cut the folder path
            dirRec = dbFileRecord_capi(folder, parentPath);

            isDir = true;
        }
        if( dirRec.isValid() && !dirRec._remotePerm.isNull()) {
            if( (isDir && !dirRec._remotePerm.contains("K"))
                    || (!isDir && !dirRec._remotePerm.contains("C")) ) {
                status.set(SyncFileStatus::STATUS_ERROR);
            }
        }
    }
    return status;
}


} // namespace OCC

