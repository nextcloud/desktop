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
#include "syncengine.h"
#include "syncfileitem.h"
#include "filesystem.h"
#include "version.h"
#include "account.h"
#include "accountstate.h"
#include "account.h"
#include "capabilities.h"

#include <QDebug>
#include <QUrl>
#include <QMetaObject>
#include <QStringList>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QLocalSocket>
#include <QStringBuilder>

#include <sqlite3.h>


#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QStandardPaths>
#endif


// This is the version that is returned when the client asks for the VERSION.
// The first number should be changed if there is an incompatible change that breaks old clients.
// The second number should be changed when there are new features.
#define MIRALL_SOCKET_API_VERSION "1.0"

static inline QString removeTrailingSlash(QString path)
{
    Q_ASSERT(path.endsWith(QLatin1Char('/')));
    path.truncate(path.length()-1);
    return path;
}

namespace OCC {

#define DEBUG qDebug() << "SocketApi: "

SocketApi::SocketApi(QObject* parent)
    : QObject(parent)
{
    QString socketPath;

    if (Utility::isWindows()) {
        socketPath = QLatin1String("\\\\.\\pipe\\")
                + QLatin1String("ownCloud-")
                + QString::fromLocal8Bit(qgetenv("USERNAME"));
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
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer.close();
    // All remaining sockets will be destroyed with _localServer, their parent
    Q_ASSERT(_listeners.isEmpty() || _listeners.first()->parent() == &_localServer);
    _listeners.clear();
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
        if (f->canSync()) {
            QString message = buildRegisterPathMessage(removeTrailingSlash(f->path()));
            sendMessage(socket, message);            
        }
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
        // Make sure to normalize the input from the socket to
        // make sure that the path will match, especially on OS X.
        QString line = QString::fromUtf8(socket->readLine()).normalized(QString::NormalizationForm_C);
        line.chop(1); // remove the '\n'
        QByteArray command = line.split(":").value(0).toAscii();
        QByteArray functionWithArguments = "command_" + command + "(QString,SocketListener*)";
        int indexOfMethod = staticMetaObject.indexOfMethod(functionWithArguments);

        QString argument = line.remove(0, command.length()+1);
        if(indexOfMethod != -1) {
            staticMetaObject.method(indexOfMethod).invoke(this, Q_ARG(QString, argument), Q_ARG(QIODevice*, socket));
        } else {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::slotRegisterPath( const QString& alias )
{
    // Make sure not to register twice to each connected client
    if (_registeredAliases.contains(alias))
        return;

    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        QString message = buildRegisterPathMessage(removeTrailingSlash(f->path()));
        foreach(QIODevice *socket, _listeners) {
            sendMessage(socket, message);
        }
    }

    _registeredAliases.insert(alias);
}

void SocketApi::slotUnregisterPath( const QString& alias )
{
    if (!_registeredAliases.contains(alias))
        return;

    Folder *f = FolderMan::instance()->folder(alias);
    if (f)
        broadcastMessage(QLatin1String("UNREGISTER_PATH"), removeTrailingSlash(f->path()), QString::null, true );

    _registeredAliases.remove(alias);
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

            QString rootPath = removeTrailingSlash(f->path());
            broadcastMessage(QLatin1String("STATUS"), rootPath,
                             f->syncEngine().syncFileStatusTracker().fileStatus("").toSocketAPIString());

            broadcastMessage(QLatin1String("UPDATE_VIEW"), rootPath);
        } else {
            qDebug() << "Not sending UPDATE_VIEW for" << f->alias() << "because status() is" << f->syncResult().status();
        }
    }
}

void SocketApi::slotFileStatusChanged(const QString& systemFileName, SyncFileStatus fileStatus)
{
    broadcastMessage(QLatin1String("STATUS"), systemFileName, fileStatus.toSocketAPIString());
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
    qDebug() << Q_FUNC_INFO << argument;

    QString statusString;

    Folder* syncFolder = FolderMan::instance()->folderForPath( argument );
    if (!syncFolder) {
        // this can happen in offline mode e.g.: nothing to worry about
        statusString = QLatin1String("NOP");
    } else {
        QString relativePath = QDir::cleanPath(argument).mid(syncFolder->cleanPath().length()+1);
        if( relativePath.endsWith(QLatin1Char('/')) ) {
            relativePath.truncate(relativePath.length()-1);
            qWarning() << "Removed trailing slash for directory: " << relativePath << "Status pushes won't have one.";
        }
        SyncFileStatus fileStatus = syncFolder->syncEngine().syncFileStatusTracker().fileStatus(relativePath);

        statusString = fileStatus.toSocketAPIString();
    }

    const QString message = QLatin1String("STATUS:") % statusString % QLatin1Char(':') %  QDir::toNativeSeparators(argument);
    sendMessage(socket, message);
}

void SocketApi::command_SHARE(const QString& localFile, QIODevice* socket)
{
    qDebug() << Q_FUNC_INFO << localFile;

    auto theme = Theme::instance();

    Folder *shareFolder = FolderMan::instance()->folderForPath(localFile);
    if (!shareFolder) {
        const QString message = QLatin1String("SHARE:NOP:")+QDir::toNativeSeparators(localFile);
        // files that are not within a sync folder are not synced.
        sendMessage(socket, message);
    } else if (!shareFolder->accountState()->isConnected()) {
        const QString message = QLatin1String("SHARE:NOTCONNECTED:")+QDir::toNativeSeparators(localFile);
        // if the folder isn't connected, don't open the share dialog
        sendMessage(socket, message);
    } else if (!theme->linkSharing() && (
                 !theme->userGroupSharing() ||
                 shareFolder->accountState()->account()->serverVersionInt() < ((8 << 16) + (2 << 8)))) {
        const QString message = QLatin1String("SHARE:NOP:")+QDir::toNativeSeparators(localFile);
        sendMessage(socket, message);
    } else {
        const QString localFileClean = QDir::cleanPath(localFile);
        const QString file = localFileClean.mid(shareFolder->cleanPath().length()+1);
        SyncFileStatus fileStatus = shareFolder->syncEngine().syncFileStatusTracker().fileStatus(file);

        // Verify the file is on the server (to our knowledge of course)
        if (fileStatus.tag() != SyncFileStatus::StatusUpToDate) {
            const QString message = QLatin1String("SHARE:NOTSYNCED:")+QDir::toNativeSeparators(localFile);
            sendMessage(socket, message);
            return;
        }

        const QString remotePath = QDir(shareFolder->remotePath()).filePath(file);

        // Can't share root folder
        if (remotePath == "/") {
            const QString message = QLatin1String("SHARE:CANNOTSHAREROOT:")+QDir::toNativeSeparators(localFile);
            sendMessage(socket, message);
            return;
        }

        SyncJournalFileRecord rec = shareFolder->journalDb()->getFileRecord(localFileClean);

        bool allowReshare = true; // lets assume the good
        if( rec.isValid() ) {
            // check the permission: Is resharing allowed?
            if( !rec._remotePerm.contains('R') ) {
                allowReshare = false;
            }
        }
        const QString message = QLatin1String("SHARE:OK:")+QDir::toNativeSeparators(localFile);
        sendMessage(socket, message);

        emit shareCommandReceived(remotePath, localFileClean, allowReshare);
    }
}

void SocketApi::command_VERSION(const QString&, QIODevice* socket)
{
    sendMessage(socket, QLatin1String("VERSION:" MIRALL_VERSION_STRING ":" MIRALL_SOCKET_API_VERSION));
}

void SocketApi::command_SHARE_STATUS(const QString &localFile, QIODevice *socket)
{
    qDebug() << Q_FUNC_INFO << localFile;

    Folder *shareFolder = FolderMan::instance()->folderForPath(localFile);

    if (!shareFolder) {
        const QString message = QLatin1String("SHARE_STATUS:NOP:")+QDir::toNativeSeparators(localFile);
        sendMessage(socket, message);
    } else {
        const QString file = QDir::cleanPath(localFile).mid(shareFolder->cleanPath().length()+1);
        SyncFileStatus fileStatus = shareFolder->syncEngine().syncFileStatusTracker().fileStatus(file);

        // Verify the file is on the server (to our knowledge of course)
        if (fileStatus.tag() != SyncFileStatus::StatusUpToDate) {
            const QString message = QLatin1String("SHARE_STATUS:NOTSYNCED:")+QDir::toNativeSeparators(localFile);
            sendMessage(socket, message);
            return;
        }

        const Capabilities capabilities = shareFolder->accountState()->account()->capabilities();

        if (!capabilities.shareAPI()) {
            const QString message = QLatin1String("SHARE_STATUS:DISABLED:")+QDir::toNativeSeparators(localFile);
            sendMessage(socket, message);
        } else {
            auto theme = Theme::instance();
            QString available;

            if (theme->userGroupSharing()) {
                available = "USER,GROUP";
            }

            if (theme->linkSharing() && capabilities.sharePublicLink()) {
                if (available.isEmpty()) {
                    available = "LINK";
                } else {
                    available += ",LINK";
                }
            }

            if (available.isEmpty()) {
                const QString message = QLatin1String("SHARE_STATUS:DISABLED") + ":" + QDir::toNativeSeparators(localFile);
                sendMessage(socket, message);
            } else {
                const QString message = QLatin1String("SHARE_STATUS:") + available + ":" + QDir::toNativeSeparators(localFile);
                sendMessage(socket, message);
            }
        }
    }
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

} // namespace OCC
