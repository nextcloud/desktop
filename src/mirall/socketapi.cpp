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

#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QStringList>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QApplication>

extern "C" {

enum csync_exclude_type_e {
  CSYNC_NOT_EXCLUDED   = 0,
  CSYNC_FILE_SILENTLY_EXCLUDED,
  CSYNC_FILE_EXCLUDE_AND_REMOVE,
  CSYNC_FILE_EXCLUDE_LIST,
  CSYNC_FILE_EXCLUDE_INVALID_CHAR
};
typedef enum csync_exclude_type_e CSYNC_EXCLUDE_TYPE;

CSYNC_EXCLUDE_TYPE csync_excluded(CSYNC *ctx, const char *path, int filetype);

}

namespace {
    const int PORT = 33001;
}

namespace Mirall {

#define DEBUG qDebug() << "SocketApi: "

namespace SocketApiHelper {

SyncFileStatus fileStatus(Folder *folder, const QString& fileName );

/**
 * @brief recursiveFolderStatus
 * @param fileName - the relative file name to examine
 * @return the resulting status
 *
 * The resulting status can only be either SYNC which means all files
 * are in sync, ERROR if an error occured, or EVAL if something needs
 * to be synced underneath this dir.
 */
// compute the file status of a directory recursively. It returns either
// "all in sync" or "needs update" or "error", no more details.
SyncFileStatus recursiveFolderStatus(Folder *folder, const QString& fileName )
{
    QDir dir(folder->path() + fileName);

    const QStringList dirEntries = dir.entryList( QDir::AllEntries | QDir::NoDotAndDotDot );

    SyncFileStatus result(SyncFileStatus::STATUS_SYNC);

    foreach( const QString entry, dirEntries ) {
        QFileInfo fi(entry);
        SyncFileStatus sfs;
        if( fi.isDir() ) {
            sfs = recursiveFolderStatus(folder, fileName + QLatin1Char('/') + entry );
        } else {
            QString fs( fileName + QLatin1Char('/') + entry );
            if( fileName.isEmpty() ) {
                // toplevel, no slash etc. needed.
                fs = entry;
            }
            sfs = fileStatus(folder, fs );
        }

        if( sfs.tag() == SyncFileStatus::STATUS_STAT_ERROR || sfs.tag() == SyncFileStatus::STATUS_ERROR ) {
            return SyncFileStatus::STATUS_ERROR;
        } else if( sfs.tag() == SyncFileStatus::STATUS_EVAL || sfs.tag() == SyncFileStatus::STATUS_NEW) {
            result.set(SyncFileStatus::STATUS_EVAL);
        }
    }
    return result;
}

/**
 * Get status about a single file.
 */
SyncFileStatus fileStatus(Folder *folder, const QString& fileName )
{
    // FIXME: Find a way for STATUS_ERROR

    QString file = folder->path();

    bool isSyncRootFolder = true;
    if( fileName != QLatin1String("/") && !fileName.isEmpty() ) {
        file = folder->path() + fileName;
        isSyncRootFolder = false;
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
    int type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    // on windows, there might be a colon in the file name.
    QRegExp rx( "^[a-zA-Z]\\:[\\\\/]+");
    if( file.contains(rx) ) {
        file.remove(0, 2);
    }
    CSYNC_EXCLUDE_TYPE excl = csync_excluded(folder->csyncContext(), file.toUtf8(), type);
    if( excl != CSYNC_NOT_EXCLUDED ) {
        return SyncFileStatus(SyncFileStatus::STATUS_IGNORE);
    }

    // Problem: for the sync dir itself we do not have a record in the sync journal
    // so the next check must not be used for the sync root folder.
    SyncJournalFileRecord rec = folder->journalDb()->getFileRecord(fileName);
    if( !isSyncRootFolder && !rec.isValid() ) {
        return SyncFileStatus(SyncFileStatus::STATUS_NEW);
    }

    SyncFileStatus status(SyncFileStatus::STATUS_NONE);
    if( type == CSYNC_FTW_TYPE_DIR ) {
        // compute recursive status of the directory
        status = recursiveFolderStatus( folder, fileName );
    } else if(fi.lastModified() != rec._modtime ) {
        // file was locally modified.
        status.set(SyncFileStatus::STATUS_EVAL);
    } else {
        status.set(SyncFileStatus::STATUS_SYNC);
    }

    if (rec._remotePerm.contains("S")) {
        // FIXME!  that should be an additional flag
       status.setSharedWithMe(true);
    }

    return status;
}


}

SocketApi::SocketApi(QObject* parent)
    : QObject(parent)
    , _localServer(new QTcpServer(this))
{
    // setup socket
    DEBUG << "Establishing SocketAPI server at" << PORT;
    if (!_localServer->listen(QHostAddress::LocalHost, PORT)) {
        DEBUG << "Failed to bind to port" << PORT;
    }
    connect(_localServer, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    // folder watcher
    connect(FolderMan::instance(), SIGNAL(folderSyncStateChange(QString)), this, SLOT(slotUpdateFolderView(QString)));
    connect(ProgressDispatcher::instance(), SIGNAL(jobCompleted(QString,SyncFileItem)),
            SLOT(slotJobCompleted(QString,SyncFileItem)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer->close();
}

void SocketApi::slotNewConnection()
{
    QTcpSocket* socket = _localServer->nextPendingConnection();

    if( ! socket ) {
        return;
    }
    DEBUG << "New connection" << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(slotReadSocket()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    Q_ASSERT(socket->readAll().isEmpty());

    _listeners.append(socket);

    foreach( QString alias, FolderMan::instance()->map().keys() ) {
       slotRegisterPath(alias);
    }
}

void SocketApi::onLostConnection()
{
    DEBUG << "Lost connection " << sender();

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    _listeners.removeAll(socket);
}


void SocketApi::slotReadSocket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,QTcpSocket*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1) {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(QTcpSocket*, socket));
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
        broadcastMessage(QLatin1String("UNREGISTER_PATH"), f->path() );
    }
}

void SocketApi::slotUpdateFolderView(const QString& alias)
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        // do only send UPDATE_VIEW for a couple of status
        if( f->syncResult().status() == SyncResult::SyncPrepare ||
                f->syncResult().status() == SyncResult::Success ||
                f->syncResult().status() == SyncResult::Problem ||
                f->syncResult().status() == SyncResult::Error   ||
                f->syncResult().status() == SyncResult::SetupError ) {
            broadcastMessage(QLatin1String("UPDATE_VIEW"), f->path() );
        }
    }
}

void SocketApi::slotJobCompleted(const QString &folder, const SyncFileItem &item)
{
    Folder *f = FolderMan::instance()->folder(folder);
    if (!f)
        return;

    const QString path = f->path() + item.destination();

    QString command = QLatin1String("OK");
    if (Progress::isWarningKind(item._status)) {
        command = QLatin1String("ERROR");
    }

    broadcastMessage(QLatin1String("BROADCAST:"), path, command);
}



void SocketApi::sendMessage(QTcpSocket *socket, const QString& message)
{
    DEBUG << "Sending message: " << message;
    QString localMessage = message;
    if( ! localMessage.endsWith(QLatin1Char('\n'))) {
        localMessage.append(QLatin1Char('\n'));
    }
    qint64 sent = socket->write(localMessage.toUtf8());
    if( sent != localMessage.toUtf8().length() ) {
        qDebug() << "WARN: Could not send all data on socket for " << localMessage;
    }

}

void SocketApi::broadcastMessage( const QString& verb, const QString& path, const QString& status )
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

    DEBUG << "Broadcasting to" << _listeners.count() << "listeners: " << msg;
    foreach(QTcpSocket *socket, _listeners) {
        sendMessage(socket, msg);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString& argument, QTcpSocket* socket)
{
    // This command is the same as RETRIEVE_FILE_STATUS

    qDebug() << Q_FUNC_INFO << argument;
    command_RETRIEVE_FILE_STATUS(argument, socket);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString& argument, QTcpSocket* socket)
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
        SyncFileStatus fileStatus = SocketApiHelper::fileStatus(syncFolder, file);

        statusString = fileStatus.toSocketAPIString();
    }

    QString message = QLatin1String("STATUS:")+statusString+QLatin1Char(':')
            +QDir::toNativeSeparators(argument);
    sendMessage(socket, message);
}

} // namespace Mirall
