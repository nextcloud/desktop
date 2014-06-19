/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
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

#include "mirall/mirallconfigfile.h"
#include "mirall/folderman.h"
#include "mirall/folder.h"
#include "mirall/utility.h"
#include "mirall/theme.h"
#include "syncjournalfilerecord.h"

#include <QDebug>
#include <QUrl>
#include <QLocalSocket>
#include <QLocalServer>
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

    SyncFileStatus result = FILE_STATUS_SYNC;

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

        if( sfs == FILE_STATUS_STAT_ERROR || sfs == FILE_STATUS_ERROR ) {
            return FILE_STATUS_ERROR;
        } else if( sfs == FILE_STATUS_EVAL || sfs == FILE_STATUS_NEW) {
            result = FILE_STATUS_EVAL;
        }
    }
    return result;
}

/**
 * Get status about a single file.
 */
SyncFileStatus fileStatus(Folder *folder, const QString& fileName )
{
    /*
    STATUS_NONE,
    + STATUS_EVAL,
    STATUS_REMOVE, (invalid for this case because it asks for local files)
    STATUS_RENAME,
    + STATUS_NEW,
    STATUS_CONFLICT,(probably also invalid as we know the conflict only with server involvement)
    + STATUS_IGNORE,
    + STATUS_SYNC,
    + STATUS_STAT_ERROR,
    STATUS_ERROR,
    STATUS_UPDATED
    */

    // FIXME: Find a way for STATUS_ERROR
    SyncFileStatus stat = FILE_STATUS_NONE;

    QString file = fileName;
    if( folder->path() != QLatin1String("/") ) {
        file = folder->path() + fileName;
    }

    QFileInfo fi(file);

    if( !fi.exists() ) {
        stat = FILE_STATUS_STAT_ERROR; // not really possible.
    }

    // file is ignored?
    if( fi.isSymLink() ) {
        stat = FILE_STATUS_IGNORE;
    }
    int type = CSYNC_FTW_TYPE_FILE;
    if( fi.isDir() ) {
        type = CSYNC_FTW_TYPE_DIR;
    }

    if( stat == FILE_STATUS_NONE ) {
        CSYNC_EXCLUDE_TYPE excl = csync_excluded(folder->csyncContext(), file.toUtf8(), type);

        if( excl != CSYNC_NOT_EXCLUDED ) {
            stat = FILE_STATUS_IGNORE;
        }
    }

    if( type == CSYNC_FTW_TYPE_DIR ) {
        // compute recursive status of the directory
        stat = recursiveFolderStatus( folder, fileName );
    } else {
        if( stat == FILE_STATUS_NONE ) {
            SyncJournalFileRecord rec = folder->journalDb()->getFileRecord(fileName);
            if( !rec.isValid() ) {
                stat = FILE_STATUS_NEW;
            } else if( stat == FILE_STATUS_NONE && fi.lastModified() != rec._modtime ) {
                // file was locally modified.
                stat = FILE_STATUS_EVAL;
            } else {
                stat = FILE_STATUS_SYNC;
            }
        }
    }
    return stat;
}


}


SocketApi::SocketApi(QObject* parent, const QUrl& localFile)
    : QObject(parent)
    , _localServer(0)
{
    QString socketPath;
    if (Utility::isWindows()) {
        socketPath = QLatin1String("\\\\.\\pipe\\")
                + Theme::instance()->appName();
    } else {
        socketPath = localFile.toLocalFile();

    }

    // setup socket
    _localServer = new QLocalServer(this);
    QLocalServer::removeServer(socketPath);
    if(!_localServer->listen(socketPath)) {
        DEBUG << "can't start server" << socketPath;
    } else {
        DEBUG << "server started, listening at " << socketPath;
    }
    connect(_localServer, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    // folder watcher
    connect(FolderMan::instance(), SIGNAL(folderSyncStateChange(QString)), SLOT(slotSyncStateChanged(QString)));
    connect(ProgressDispatcher::instance(), SIGNAL(jobCompleted(QString,SyncFileItem)), SLOT(slotJobCompleted(QString,SyncFileItem)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer->close();
}

void SocketApi::slotNewConnection()
{
    QLocalSocket* socket = _localServer->nextPendingConnection();
    if( ! socket ) {
        return;
    }
    DEBUG << "New connection " << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(slotReadSocket()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    Q_ASSERT(socket->readAll().isEmpty());

    _listeners.append(socket);
}

void SocketApi::onLostConnection()
{
    DEBUG << "Lost connection " << sender();

    QLocalSocket* socket = qobject_cast< QLocalSocket* >(sender());
    _listeners.removeAll(socket);
}


void SocketApi::slotReadSocket()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine()) {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,QLocalSocket*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1) {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(QLocalSocket*, socket));
        } else {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::slotSyncStateChanged(const QString&)
{
    broadcastMessage("UPDATE_VIEW");
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

    broadcastMessage(QLatin1String("BROADCAST:") + command + QLatin1Char(':') + path);
}



void SocketApi::sendMessage(QLocalSocket* socket, const QString& message)
{
    DEBUG << "Sending message: " << message;
    QString localMessage = message;
    socket->write(localMessage.append("\n").toUtf8());
}

void SocketApi::broadcastMessage(const QString& message)
{
    DEBUG << "Broadcasting to" << _listeners.count() << "listeners: " << message;
    foreach(QLocalSocket* current, _listeners)
    {
        sendMessage(current, message);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString& argument, QLocalSocket* socket)
{
    // This command is the same as RETRIEVE_FILE_STATUS

    qDebug() << Q_FUNC_INFO << argument;
    command_RETRIEVE_FILE_STATUS(argument, socket);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString& argument, QLocalSocket* socket)
{
    if( !socket ) {
        qDebug() << "No valid socket object.";
        return;
    }

    qDebug() << Q_FUNC_INFO << argument;

    QString statusString;

    Folder* folder = FolderMan::instance()->folderForPath( QUrl::fromLocalFile(argument) );
    // this can happen in offline mode e.g.: nothing to worry about
    if (!folder) {
        DEBUG << "folder offline or not watched:" << argument;
        statusString = QLatin1String("NOP");
    }

    if( statusString.isEmpty() ) {
        SyncFileStatus fileStatus = SocketApiHelper::fileStatus(folder, argument.mid(folder->path().length()) );
        if( fileStatus == FILE_STATUS_STAT_ERROR ) {
            qDebug() << "XXXXXXXXXXXX FileStatus is STAT ERROR for " << argument;
        }
        if( fileStatus != FILE_STATUS_SYNC ) {
            qDebug() << "SyncFileStatus for " << argument << " is " << fileStatus;
            // we found something that is not in sync
            statusString = QLatin1String("NEED_SYNC");
        }
    }

    if( statusString.isEmpty() ) {
        statusString = QLatin1String("OK");
    }

    QString message = QLatin1String("STATUS:")+statusString+QLatin1Char(':')+argument;
    sendMessage(socket, message);
}

} // namespace Mirall
