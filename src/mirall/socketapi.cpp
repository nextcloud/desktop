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

#include <QDebug>
#include <QUrl>
#include <QLocalSocket>
#include <QLocalServer>
#include <QMetaObject>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QApplication>

#include "mirall/utility.h"

namespace Mirall {

#define DEBUG qDebug() << "SocketApi: "

SocketApi::SocketApi(QObject* parent, const QUrl& localFile)
    : QObject(parent)
    , _localServer(0)
{
    QString socketPath;
    if (Utility::isWindows()) {
        socketPath = QLatin1String("\\\\.\\pipe\\");
    } else {
        socketPath = localFile.toLocalFile();

    }
    DEBUG << "ctor: " << socketPath;

    // setup socket
    _localServer = new QLocalServer(this);
    QLocalServer::removeServer(socketPath);
    if(!_localServer->listen(socketPath))
        DEBUG << "can't start server";
    else
        DEBUG << "server started";
    connect(_localServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

    // folder watcher
    connect(FolderMan::instance(), SIGNAL(folderSyncStateChange(QString)), SLOT(onSyncStateChanged(QString)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer->close();
}

void SocketApi::onNewConnection()
{
    QLocalSocket* socket = _localServer->nextPendingConnection();
    DEBUG << "New connection " << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
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


void SocketApi::onReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine())
    {
        QString line = QString::fromUtf8(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,QLocalSocket*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1)
        {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(QLocalSocket*, socket));
        }
        else
        {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::onSyncStateChanged(const QString&)
{
    DEBUG << "Sync state changed";

    broadcastMessage("UPDATE_VIEW");
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
    qDebug() << Q_FUNC_INFO << argument;
    //TODO: do security checks?!
    Folder* folder = FolderMan::instance()->folderForPath( argument );
    // this can happen in offline mode e.g.: nothing to worry about
    if(!folder)
    {
        DEBUG << "folder offline or not watched:" << argument;
        return;
    }

    QDir dir(argument);
    foreach(QString entry, dir.entryList(QDir::AllEntries|QDir::NoDotAndDotDot))
    {
        QString absoluteFilePath = dir.absoluteFilePath(entry);
        QString statusString;
        SyncFileStatus fileStatus = folder->fileStatus(absoluteFilePath.mid(folder->path().length()));
        switch(fileStatus)
        {
            case FILE_STATUS_NONE:
                statusString = QLatin1String("STATUS_NONE");
                break;
            case FILE_STATUS_EVAL:
                statusString = QLatin1String("STATUS_EVAL");
                break;
            case FILE_STATUS_REMOVE:
                statusString = QLatin1String("STATUS_REMOVE");
                break;
            case FILE_STATUS_RENAME:
                statusString = QLatin1String("STATUS_RENAME");
                break;
            case FILE_STATUS_NEW:
                statusString = QLatin1String("STATUS_NEW");
                break;
            case FILE_STATUS_CONFLICT:
                statusString = QLatin1String("STATUS_CONFLICT");
                break;
            case FILE_STATUS_IGNORE:
                statusString = QLatin1String("STATUS_IGNORE");
                break;
            case FILE_STATUS_SYNC:
                statusString = QLatin1String("STATUS_SYNC");
                break;
            case FILE_STATUS_STAT_ERROR:
                statusString = QLatin1String("STATUS_STAT_ERROR");
                break;
            case FILE_STATUS_ERROR:
                statusString = QLatin1String("STATUS_ERROR");
                break;
            case FILE_STATUS_UPDATED:
                statusString = QLatin1String("STATUS_UPDATED");
                break;
            default:
                qWarning() << "not all SyncFileStatus items checked!";
                Q_ASSERT(false);
                statusString = QLatin1String("STATUS_NONE");

        }
        QString message("%1:%2:%3");
        message = message.arg("STATUS").arg(statusString).arg(absoluteFilePath);
        sendMessage(socket, message);
    }
}

} // namespace Mirall
