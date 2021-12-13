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
#include "socketapi_p.h"
#include "socketapi/socketuploadjob.h"

#include "conflictdialog.h"
#include "conflictsolver.h"

#include "config.h"
#include "configfile.h"
#include "folderman.h"
#include "folder.h"
#include "theme.h"
#include "common/syncjournalfilerecord.h"
#include "syncengine.h"
#include "syncfileitem.h"
#include "filesystem.h"
#include "version.h"
#include "account.h"
#include "accountstate.h"
#include "account.h"
#include "accountmanager.h"
#include "capabilities.h"
#include "common/asserts.h"
#include "guiutility.h"
#ifndef OWNCLOUD_TEST
#include "sharemanager.h"
#endif

#include <array>
#include <QBitArray>
#include <QUrl>
#include <QMetaMethod>
#include <QMetaObject>
#include <QStringList>
#include <QScopedPointer>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QLocalSocket>
#include <QStringBuilder>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>


#include <QAction>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWidget>

#include <QClipboard>
#include <QDesktopServices>

#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif


// This is the version that is returned when the client asks for the VERSION.
// The first number should be changed if there is an incompatible change that breaks old clients.
// The second number should be changed when there are new features.
#define MIRALL_SOCKET_API_VERSION "1.1"

namespace {

const QLatin1Char RecordSeparator()
{
    return QLatin1Char('\x1e');
}

QStringList split(const QString &data)
{
    // TODO: string ref?
    return data.split(RecordSeparator());
}

#if GUI_TESTING

using namespace OCC;

QList<QObject *> allObjects(const QList<QWidget *> &widgets)
{
    QList<QObject *> objects;
    std::copy(widgets.constBegin(), widgets.constEnd(), std::back_inserter(objects));

    objects << qApp;

    return objects;
}

QObject *findWidget(const QString &queryString, const QList<QWidget *> &widgets = QApplication::allWidgets())
{
    auto objects = allObjects(widgets);

    QList<QObject *>::const_iterator foundWidget;

    if (queryString.contains('>')) {
        qCDebug(lcSocketApi) << "queryString contains >";

        auto subQueries = queryString.split('>', QString::SkipEmptyParts);
        Q_ASSERT(subQueries.count() == 2);

        auto parentQueryString = subQueries[0].trimmed();
        qCDebug(lcSocketApi) << "Find parent: " << parentQueryString;
        auto parent = findWidget(parentQueryString);

        if (!parent) {
            return nullptr;
        }

        auto childQueryString = subQueries[1].trimmed();
        auto child = findWidget(childQueryString, parent->findChildren<QWidget *>());
        qCDebug(lcSocketApi) << "found child: " << !!child;
        return child;

    } else if (queryString.startsWith('#')) {
        auto objectName = queryString.mid(1);
        qCDebug(lcSocketApi) << "find objectName: " << objectName;
        foundWidget = std::find_if(objects.constBegin(), objects.constEnd(), [&](QObject *widget) {
            return widget->objectName() == objectName;
        });
    } else {
        QList<QObject *> matches;
        std::copy_if(objects.constBegin(), objects.constEnd(), std::back_inserter(matches), [&](QObject *widget) {
            return widget->inherits(queryString.toLatin1());
        });

        std::for_each(matches.constBegin(), matches.constEnd(), [](QObject *w) {
            if (!w)
                return;
            qCDebug(lcSocketApi) << "WIDGET: " << w->objectName() << w->metaObject()->className();
        });

        if (matches.empty()) {
            return nullptr;
        }
        return matches[0];
    }

    if (foundWidget == objects.constEnd()) {
        return nullptr;
    }

    return *foundWidget;
}
#endif

static inline QString removeTrailingSlash(QString path)
{
    Q_ASSERT(path.endsWith(QLatin1Char('/')));
    path.truncate(path.length() - 1);
    return path;
}

static QString buildMessage(const QString &verb, const QString &path, const QString &status = QString())
{
    QString msg(verb);

    if (!status.isEmpty()) {
        msg.append(QLatin1Char(':'));
        msg.append(status);
    }
    if (!path.isEmpty()) {
        msg.append(QLatin1Char(':'));
        QFileInfo fi(path);
        msg.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    }
    return msg;
}
}

namespace OCC {

Q_LOGGING_CATEGORY(lcSocketApi, "nextcloud.gui.socketapi", QtInfoMsg)
Q_LOGGING_CATEGORY(lcPublicLink, "nextcloud.gui.socketapi.publiclink", QtInfoMsg)


void SocketListener::sendMessage(const QString &message, bool doWait) const
{
    if (!socket) {
        qCWarning(lcSocketApi) << "Not sending message to dead socket:" << message;
        return;
    }

    qCDebug(lcSocketApi) << "Sending SocketAPI message -->" << message << "to" << socket;
    QString localMessage = message;
    if (!localMessage.endsWith(QLatin1Char('\n'))) {
        localMessage.append(QLatin1Char('\n'));
    }

    QByteArray bytesToSend = localMessage.toUtf8();
    qint64 sent = socket->write(bytesToSend);
    if (doWait) {
        socket->waitForBytesWritten(1000);
    }
    if (sent != bytesToSend.length()) {
        qCWarning(lcSocketApi) << "Could not send all data on socket for " << localMessage;
    }
}

SocketApi::SocketApi(QObject *parent)
    : QObject(parent)
{
    QString socketPath;

    qRegisterMetaType<SocketListener *>("SocketListener*");
    qRegisterMetaType<QSharedPointer<SocketApiJob>>("QSharedPointer<SocketApiJob>");
    qRegisterMetaType<QSharedPointer<SocketApiJobV2>>("QSharedPointer<SocketApiJobV2>");

    if (Utility::isWindows()) {
        socketPath = QLatin1String(R"(\\.\pipe\)")
            + QLatin1String(APPLICATION_EXECUTABLE)
            + QLatin1String("-")
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
#ifdef Q_OS_MAC
        CFURLRef url = (CFURLRef)CFAutorelease((CFURLRef)CFBundleCopyBundleURL(CFBundleGetMainBundle()));
        QString bundlePath = QUrl::fromCFURL(url).path();

        auto _system = [](const QString &cmd, const QStringList &args) {
            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(cmd, args);
            if (!process.waitForFinished()) {
                qCWarning(lcSocketApi) << "Failed to load shell extension:" << cmd << args.join(" ") << process.errorString();
            } else {
                qCInfo(lcSocketApi) << (process.exitCode() != 0 ? "Failed to load" : "Loaded") << "shell extension:" << cmd << args.join(" ") << process.readAll();
            }
        };
        // Add it again. This was needed for Mojave to trigger a load.
        _system(QStringLiteral("pluginkit"), { QStringLiteral("-a"), QStringLiteral("%1Contents/PlugIns/FinderSyncExt.appex/").arg(bundlePath) });
        // Tell Finder to use the Extension (checking it from System Preferences -> Extensions)
        _system(QStringLiteral("pluginkit"), { QStringLiteral("-e"), QStringLiteral("use"), QStringLiteral("-i"), QStringLiteral(APPLICATION_REV_DOMAIN ".FinderSyncExt") });

#endif
    } else if (Utility::isLinux() || Utility::isBSD()) {
        QString runtimeDir;
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        socketPath = runtimeDir + "/" + Theme::instance()->appName() + "/socket";
    } else {
        qCWarning(lcSocketApi) << "An unexpected system detected, this probably won't work.";
    }

    SocketApiServer::removeServer(socketPath);
    QFileInfo info(socketPath);
    if (!info.dir().exists()) {
        bool result = info.dir().mkpath(".");
        qCDebug(lcSocketApi) << "creating" << info.dir().path() << result;
        if (result) {
            QFile::setPermissions(socketPath,
                QFile::Permissions(QFile::ReadOwner + QFile::WriteOwner + QFile::ExeOwner));
        }
    }
    if (!_localServer.listen(socketPath)) {
        qCWarning(lcSocketApi) << "can't start server" << socketPath;
    } else {
        qCInfo(lcSocketApi) << "server started, listening at " << socketPath;
    }

    connect(&_localServer, &SocketApiServer::newConnection, this, &SocketApi::slotNewConnection);

    // folder watcher
    connect(FolderMan::instance(), &FolderMan::folderSyncStateChange, this, &SocketApi::slotUpdateFolderView);
}

SocketApi::~SocketApi()
{
    qCDebug(lcSocketApi) << "dtor";
    _localServer.close();
    // All remaining sockets will be destroyed with _localServer, their parent
    ASSERT(_listeners.isEmpty() || _listeners.first()->socket->parent() == &_localServer)
    _listeners.clear();
}

void SocketApi::slotNewConnection()
{
    // Note that on macOS this is not actually a line-based QIODevice, it's a SocketApiSocket which is our
    // custom message based macOS IPC.
    QIODevice *socket = _localServer.nextPendingConnection();

    if (!socket) {
        return;
    }
    qCInfo(lcSocketApi) << "New connection" << socket;
    connect(socket, &QIODevice::readyRead, this, &SocketApi::slotReadSocket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    connect(socket, &QObject::destroyed, this, &SocketApi::slotSocketDestroyed);
    ASSERT(socket->readAll().isEmpty());

    auto listener = QSharedPointer<SocketListener>::create(socket);
    _listeners.insert(socket, listener);
    for (Folder *f : FolderMan::instance()->map()) {
        if (f->canSync()) {
            QString message = buildRegisterPathMessage(removeTrailingSlash(f->path()));
            qCInfo(lcSocketApi) << "Trying to send SocketAPI Register Path Message -->" << message << "to" << listener->socket;
            listener->sendMessage(message);
        }
    }
}

void SocketApi::onLostConnection()
{
    qCInfo(lcSocketApi) << "Lost connection " << sender();
    sender()->deleteLater();

    auto socket = qobject_cast<QIODevice *>(sender());
    ASSERT(socket);
    _listeners.remove(socket);
}

void SocketApi::slotSocketDestroyed(QObject *obj)
{
    auto *socket = static_cast<QIODevice *>(obj);
    _listeners.remove(socket);
}

void SocketApi::slotReadSocket()
{
    auto *socket = qobject_cast<QIODevice *>(sender());
    ASSERT(socket);

    // Find the SocketListener
    //
    // It's possible for the disconnected() signal to be triggered before
    // the readyRead() signals are received - in that case there won't be a
    // valid listener. We execute the handler anyway, but it will work with
    // a SocketListener that doesn't send any messages.
    static auto invalidListener = QSharedPointer<SocketListener>::create(nullptr);
    const auto listener = _listeners.value(socket, invalidListener);
    while (socket->canReadLine()) {
        // Make sure to normalize the input from the socket to
        // make sure that the path will match, especially on OS X.
        const QString line = QString::fromUtf8(socket->readLine().trimmed()).normalized(QString::NormalizationForm_C);
        qCInfo(lcSocketApi) << "Received SocketAPI message <--" << line << "from" << socket;
        const int argPos = line.indexOf(QLatin1Char(':'));
        const QByteArray command = line.midRef(0, argPos).toUtf8().toUpper();
        const int indexOfMethod = [&] {
            QByteArray functionWithArguments = QByteArrayLiteral("command_");
            if (command.startsWith("ASYNC_")) {
                functionWithArguments += command + QByteArrayLiteral("(QSharedPointer<SocketApiJob>)");
            } else if (command.startsWith("V2/")) {
                functionWithArguments += QByteArrayLiteral("V2_") + command.mid(3) + QByteArrayLiteral("(QSharedPointer<SocketApiJobV2>)");
            } else {
                functionWithArguments += command + QByteArrayLiteral("(QString,SocketListener*)");
            }
            Q_ASSERT(staticQtMetaObject.normalizedSignature(functionWithArguments) == functionWithArguments);
            const auto out = staticMetaObject.indexOfMethod(functionWithArguments);
            if (out == -1) {
                listener->sendError(QStringLiteral("Function %1 not found").arg(QString::fromUtf8(functionWithArguments)));
            }
            ASSERT(out != -1)
            return out;
        }();

        const auto argument = argPos != -1 ? line.midRef(argPos + 1) : QStringRef();
        if (command.startsWith("ASYNC_")) {
            auto arguments = argument.split('|');
            if (arguments.size() != 2) {
                listener->sendError(QStringLiteral("argument count is wrong"));
                return;
            }

            auto json = QJsonDocument::fromJson(arguments[1].toUtf8()).object();

            auto jobId = arguments[0];

            auto socketApiJob = QSharedPointer<SocketApiJob>(
                new SocketApiJob(jobId.toString(), listener, json), &QObject::deleteLater);
            if (indexOfMethod != -1) {
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::QueuedConnection,
                        Q_ARG(QSharedPointer<SocketApiJob>, socketApiJob));
            } else {
                qCWarning(lcSocketApi) << "The command is not supported by this version of the client:" << command
                                       << "with argument:" << argument;
                socketApiJob->reject(QStringLiteral("command not found"));
            }
        } else if (command.startsWith("V2/")) {
            QJsonParseError error;
            const auto json = QJsonDocument::fromJson(argument.toUtf8(), &error).object();
            if (error.error != QJsonParseError::NoError) {
                qCWarning(lcSocketApi()) << "Invalid json" << argument.toString() << error.errorString();
                listener->sendError(error.errorString());
                return;
            }
            auto socketApiJob = QSharedPointer<SocketApiJobV2>::create(listener, command, json);
            if (indexOfMethod != -1) {
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::QueuedConnection,
                        Q_ARG(QSharedPointer<SocketApiJobV2>, socketApiJob));
            } else {
                qCWarning(lcSocketApi) << "The command is not supported by this version of the client:" << command
                                       << "with argument:" << argument;
                socketApiJob->failure(QStringLiteral("command not found"));
            }
        } else {
            if (indexOfMethod != -1) {
                // to ensure that listener is still valid we need to call it with Qt::DirectConnection
                ASSERT(thread() == QThread::currentThread())
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::DirectConnection, Q_ARG(QString, argument.toString()),
                        Q_ARG(SocketListener *, listener.data()));
            }
        }
    }
}

void SocketApi::slotRegisterPath(const QString &alias)
{
    // Make sure not to register twice to each connected client
    if (_registeredAliases.contains(alias))
        return;

    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        const QString message = buildRegisterPathMessage(removeTrailingSlash(f->path()));
        for (const auto &listener : qAsConst(_listeners)) {
            qCInfo(lcSocketApi) << "Trying to send SocketAPI Register Path Message -->" << message << "to" << listener->socket;
            listener->sendMessage(message);
        }
    }

    _registeredAliases.insert(alias);
}

void SocketApi::slotUnregisterPath(const QString &alias)
{
    if (!_registeredAliases.contains(alias))
        return;

    Folder *f = FolderMan::instance()->folder(alias);
    if (f)
        broadcastMessage(buildMessage(QLatin1String("UNREGISTER_PATH"), removeTrailingSlash(f->path()), QString()), true);

    _registeredAliases.remove(alias);
}

void SocketApi::slotUpdateFolderView(Folder *f)
{
    if (_listeners.isEmpty()) {
        return;
    }

    if (f) {
        // do only send UPDATE_VIEW for a couple of status
        if (f->syncResult().status() == SyncResult::SyncPrepare
            || f->syncResult().status() == SyncResult::Success
            || f->syncResult().status() == SyncResult::Paused
            || f->syncResult().status() == SyncResult::Problem
            || f->syncResult().status() == SyncResult::Error
            || f->syncResult().status() == SyncResult::SetupError) {
            QString rootPath = removeTrailingSlash(f->path());
            broadcastStatusPushMessage(rootPath, f->syncEngine().syncFileStatusTracker().fileStatus(""));

            broadcastMessage(buildMessage(QLatin1String("UPDATE_VIEW"), rootPath));
        } else {
            qCDebug(lcSocketApi) << "Not sending UPDATE_VIEW for" << f->alias() << "because status() is" << f->syncResult().status();
        }
    }
}

void SocketApi::broadcastMessage(const QString &msg, bool doWait)
{
    for (const auto &listener : qAsConst(_listeners)) {
        listener->sendMessage(msg, doWait);
    }
}

void SocketApi::processFileActivityRequest(const QString &localFile)
{
    const auto fileData = FileData::get(localFile);
    emit fileActivityCommandReceived(fileData.serverRelativePath, fileData.localPath);
}

void SocketApi::processShareRequest(const QString &localFile, SocketListener *listener, ShareDialogStartPage startPage)
{
    auto theme = Theme::instance();

    auto fileData = FileData::get(localFile);
    auto shareFolder = fileData.folder;
    if (!shareFolder) {
        const QString message = QLatin1String("SHARE:NOP:") + QDir::toNativeSeparators(localFile);
        // files that are not within a sync folder are not synced.
        listener->sendMessage(message);
    } else if (!shareFolder->accountState()->isConnected()) {
        const QString message = QLatin1String("SHARE:NOTCONNECTED:") + QDir::toNativeSeparators(localFile);
        // if the folder isn't connected, don't open the share dialog
        listener->sendMessage(message);
    } else if (!theme->linkSharing() && (!theme->userGroupSharing() || shareFolder->accountState()->account()->serverVersionInt() < Account::makeServerVersion(8, 2, 0))) {
        const QString message = QLatin1String("SHARE:NOP:") + QDir::toNativeSeparators(localFile);
        listener->sendMessage(message);
    } else {
        // If the file doesn't have a journal record, it might not be uploaded yet
        if (!fileData.journalRecord().isValid()) {
            const QString message = QLatin1String("SHARE:NOTSYNCED:") + QDir::toNativeSeparators(localFile);
            listener->sendMessage(message);
            return;
        }

        auto &remotePath = fileData.serverRelativePath;

        // Can't share root folder
        if (remotePath == "/") {
            const QString message = QLatin1String("SHARE:CANNOTSHAREROOT:") + QDir::toNativeSeparators(localFile);
            listener->sendMessage(message);
            return;
        }

        const QString message = QLatin1String("SHARE:OK:") + QDir::toNativeSeparators(localFile);
        listener->sendMessage(message);

        emit shareCommandReceived(remotePath, fileData.localPath, startPage);
    }
}

void SocketApi::broadcastStatusPushMessage(const QString &systemPath, SyncFileStatus fileStatus)
{
    QString msg = buildMessage(QLatin1String("STATUS"), systemPath, fileStatus.toSocketAPIString());
    Q_ASSERT(!systemPath.endsWith('/'));
    uint directoryHash = qHash(systemPath.left(systemPath.lastIndexOf('/')));
    for (const auto &listener : qAsConst(_listeners)) {
        listener->sendMessageIfDirectoryMonitored(msg, directoryHash);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString &argument, SocketListener *listener)
{
    // This command is the same as RETRIEVE_FILE_STATUS
    command_RETRIEVE_FILE_STATUS(argument, listener);
}

void SocketApi::command_RETRIEVE_FILE_STATUS(const QString &argument, SocketListener *listener)
{
    QString statusString;

    auto fileData = FileData::get(argument);
    if (!fileData.folder) {
        // this can happen in offline mode e.g.: nothing to worry about
        statusString = QLatin1String("NOP");
    } else {
        // The user probably visited this directory in the file shell.
        // Let the listener know that it should now send status pushes for sibblings of this file.
        QString directory = fileData.localPath.left(fileData.localPath.lastIndexOf('/'));
        listener->registerMonitoredDirectory(qHash(directory));

        SyncFileStatus fileStatus = fileData.syncFileStatus();
        statusString = fileStatus.toSocketAPIString();
    }

    const QString message = QLatin1String("STATUS:") % statusString % QLatin1Char(':') % QDir::toNativeSeparators(argument);
    listener->sendMessage(message);
}

void SocketApi::command_SHARE(const QString &localFile, SocketListener *listener)
{
    processShareRequest(localFile, listener, ShareDialogStartPage::UsersAndGroups);
}

void SocketApi::command_ACTIVITY(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener);

    processFileActivityRequest(localFile);
}

void SocketApi::command_MANAGE_PUBLIC_LINKS(const QString &localFile, SocketListener *listener)
{
    processShareRequest(localFile, listener, ShareDialogStartPage::PublicLinks);
}

void SocketApi::command_VERSION(const QString &, SocketListener *listener)
{
    listener->sendMessage(QLatin1String("VERSION:" MIRALL_VERSION_STRING ":" MIRALL_SOCKET_API_VERSION));
}

void SocketApi::command_SHARE_MENU_TITLE(const QString &, SocketListener *listener)
{
    //listener->sendMessage(QLatin1String("SHARE_MENU_TITLE:") + tr("Share with %1", "parameter is Nextcloud").arg(Theme::instance()->appNameGUI()));
    listener->sendMessage(QLatin1String("SHARE_MENU_TITLE:") + Theme::instance()->appNameGUI());
}

void SocketApi::command_EDIT(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener)
    auto fileData = FileData::get(localFile);
    if (!fileData.folder) {
        qCWarning(lcSocketApi) << "Unknown path" << localFile;
        return;
    }

    auto record = fileData.journalRecord();
    if (!record.isValid())
        return;

    DirectEditor* editor = getDirectEditorForLocalFile(fileData.localPath);
    if (!editor)
        return;

    auto *job = new JsonApiJob(fileData.folder->accountState()->account(), QLatin1String("ocs/v2.php/apps/files/api/v1/directEditing/open"), this);

    QUrlQuery params;
    params.addQueryItem("path", fileData.serverRelativePath);
    params.addQueryItem("editorId", editor->id());
    job->addQueryParams(params);
    job->setVerb(JsonApiJob::Verb::Post);

    QObject::connect(job, &JsonApiJob::jsonReceived, [](const QJsonDocument &json){
        auto data = json.object().value("ocs").toObject().value("data").toObject();
        auto url = QUrl(data.value("url").toString());

        if(!url.isEmpty())
            Utility::openBrowser(url);
    });
    job->start();
}

// don't pull the share manager into socketapi unittests
#ifndef OWNCLOUD_TEST

class GetOrCreatePublicLinkShare : public QObject
{
    Q_OBJECT
public:
    GetOrCreatePublicLinkShare(const AccountPtr &account, const QString &localFile,
        QObject *parent)
        : QObject(parent)
        , _account(account)
        , _shareManager(account)
        , _localFile(localFile)
    {
        connect(&_shareManager, &ShareManager::sharesFetched,
            this, &GetOrCreatePublicLinkShare::sharesFetched);
        connect(&_shareManager, &ShareManager::linkShareCreated,
            this, &GetOrCreatePublicLinkShare::linkShareCreated);
        connect(&_shareManager, &ShareManager::linkShareRequiresPassword,
            this, &GetOrCreatePublicLinkShare::linkShareRequiresPassword);
        connect(&_shareManager, &ShareManager::serverError,
            this, &GetOrCreatePublicLinkShare::serverError);
    }

    void run()
    {
        qCDebug(lcPublicLink) << "Fetching shares";
        _shareManager.fetchShares(_localFile);
    }

private slots:
    void sharesFetched(const QList<QSharedPointer<Share>> &shares)
    {
        auto shareName = SocketApi::tr("Context menu share");

        // If there already is a context menu share, reuse it
        for (const auto &share : shares) {
            const auto linkShare = qSharedPointerDynamicCast<LinkShare>(share);
            if (!linkShare)
                continue;

            if (linkShare->getName() == shareName) {
                qCDebug(lcPublicLink) << "Found existing share, reusing";
                return success(linkShare->getLink().toString());
            }
        }

        // otherwise create a new one
        qCDebug(lcPublicLink) << "Creating new share";
        _shareManager.createLinkShare(_localFile, shareName, QString());
    }

    void linkShareCreated(const QSharedPointer<LinkShare> &share)
    {
        qCDebug(lcPublicLink) << "New share created";
        success(share->getLink().toString());
    }

    void passwordRequired() {
        bool ok = false;
        QString password = QInputDialog::getText(nullptr,
                                                 tr("Password for share required"),
                                                 tr("Please enter a password for your link share:"),
                                                 QLineEdit::Normal,
                                                 QString(),
                                                 &ok);

        if (!ok) {
            // The dialog was canceled so no need to do anything
            return;
        }

        // Try to create the link share again with the newly entered password
        _shareManager.createLinkShare(_localFile, QString(), password);
    }

    void linkShareRequiresPassword(const QString &message)
    {
        qCInfo(lcPublicLink) << "Could not create link share:" << message;
        emit error(message);
        deleteLater();
    }

    void serverError(int code, const QString &message)
    {
        qCWarning(lcPublicLink) << "Share fetch/create error" << code << message;
        QMessageBox::warning(
            nullptr,
            tr("Sharing error"),
            tr("Could not retrieve or create the public link share. Error:\n\n%1").arg(message),
            QMessageBox::Ok,
            QMessageBox::NoButton);
        emit error(message);
        deleteLater();
    }

signals:
    void done(const QString &link);
    void error(const QString &message);

private:
    void success(const QString &link)
    {
        emit done(link);
        deleteLater();
    }

    AccountPtr _account;
    ShareManager _shareManager;
    QString _localFile;
};

#else

class GetOrCreatePublicLinkShare : public QObject
{
    Q_OBJECT
public:
    GetOrCreatePublicLinkShare(const AccountPtr &, const QString &,
        std::function<void(const QString &link)>, QObject *)
    {
    }

    void run()
    {
    }
};

#endif

void SocketApi::command_COPY_PUBLIC_LINK(const QString &localFile, SocketListener *)
{
    auto fileData = FileData::get(localFile);
    if (!fileData.folder)
        return;

    AccountPtr account = fileData.folder->accountState()->account();
    auto job = new GetOrCreatePublicLinkShare(account, fileData.serverRelativePath, this);
    connect(job, &GetOrCreatePublicLinkShare::done, this,
        [](const QString &url) { copyUrlToClipboard(url); });
    connect(job, &GetOrCreatePublicLinkShare::error, this,
        [=]() { emit shareCommandReceived(fileData.serverRelativePath, fileData.localPath, ShareDialogStartPage::PublicLinks); });
    job->run();
}

// Windows Shell / Explorer pinning fallbacks, see issue: https://github.com/nextcloud/desktop/issues/1599
#ifdef Q_OS_WIN
void SocketApi::command_COPYASPATH(const QString &localFile, SocketListener *)
{
    QApplication::clipboard()->setText(localFile);
}

void SocketApi::command_OPENNEWWINDOW(const QString &localFile, SocketListener *)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(localFile));
}

void SocketApi::command_OPEN(const QString &localFile, SocketListener *socketListener)
{
    command_OPENNEWWINDOW(localFile, socketListener);
}
#endif

// Fetches the private link url asynchronously and then calls the target slot
void SocketApi::fetchPrivateLinkUrlHelper(const QString &localFile, const std::function<void(const QString &url)> &targetFun)
{
    auto fileData = FileData::get(localFile);
    if (!fileData.folder) {
        qCWarning(lcSocketApi) << "Unknown path" << localFile;
        return;
    }

    auto record = fileData.journalRecord();
    if (!record.isValid())
        return;

    fetchPrivateLinkUrl(
        fileData.folder->accountState()->account(),
        fileData.serverRelativePath,
        record.numericFileId(),
        this,
        targetFun);
}

void SocketApi::command_COPY_PRIVATE_LINK(const QString &localFile, SocketListener *)
{
    fetchPrivateLinkUrlHelper(localFile, &SocketApi::copyUrlToClipboard);
}

void SocketApi::command_EMAIL_PRIVATE_LINK(const QString &localFile, SocketListener *)
{
    fetchPrivateLinkUrlHelper(localFile, &SocketApi::emailPrivateLink);
}

void SocketApi::command_OPEN_PRIVATE_LINK(const QString &localFile, SocketListener *)
{
    fetchPrivateLinkUrlHelper(localFile, &SocketApi::openPrivateLink);
}

void SocketApi::command_MAKE_AVAILABLE_LOCALLY(const QString &filesArg, SocketListener *)
{
    const QStringList files = split(filesArg);

    for (const auto &file : files) {
        auto data = FileData::get(file);
        if (!data.folder)
            continue;

        // Update the pin state on all items
        if (!data.folder->vfs().setPinState(data.folderRelativePath, PinState::AlwaysLocal)) {
            qCWarning(lcSocketApi) << "Could not set pin state of" << data.folderRelativePath << "to always local";
        }

        // Trigger sync
        data.folder->schedulePathForLocalDiscovery(data.folderRelativePath);
        data.folder->scheduleThisFolderSoon();
    }
}

/* Go over all the files and replace them by a virtual file */
void SocketApi::command_MAKE_ONLINE_ONLY(const QString &filesArg, SocketListener *)
{
    const QStringList files = split(filesArg);

    for (const auto &file : files) {
        auto data = FileData::get(file);
        if (!data.folder)
            continue;

        // Update the pin state on all items
        if (!data.folder->vfs().setPinState(data.folderRelativePath, PinState::OnlineOnly)) {
            qCWarning(lcSocketApi) << "Could not set pin state of" << data.folderRelativePath << "to online only";
        }

        // Trigger sync
        data.folder->schedulePathForLocalDiscovery(data.folderRelativePath);
        data.folder->scheduleThisFolderSoon();
    }
}

void SocketApi::copyUrlToClipboard(const QString &link)
{
    QApplication::clipboard()->setText(link);
}

void SocketApi::command_RESOLVE_CONFLICT(const QString &localFile, SocketListener *)
{
    const auto fileData = FileData::get(localFile);
    if (!fileData.folder || !Utility::isConflictFile(fileData.folderRelativePath))
        return; // should not have shown menu item

    const auto conflictedRelativePath = fileData.folderRelativePath;
    const auto baseRelativePath = fileData.folder->journalDb()->conflictFileBaseName(fileData.folderRelativePath.toUtf8());

    const auto dir = QDir(fileData.folder->path());
    const auto conflictedPath = dir.filePath(conflictedRelativePath);
    const auto basePath = dir.filePath(baseRelativePath);

    const auto baseName = QFileInfo(basePath).fileName();

#ifndef OWNCLOUD_TEST
    ConflictDialog dialog;
    dialog.setBaseFilename(baseName);
    dialog.setLocalVersionFilename(conflictedPath);
    dialog.setRemoteVersionFilename(basePath);
    if (dialog.exec() == ConflictDialog::Accepted) {
        fileData.folder->scheduleThisFolderSoon();
    }
#endif
}

void SocketApi::command_DELETE_ITEM(const QString &localFile, SocketListener *)
{
    ConflictSolver solver;
    solver.setLocalVersionFilename(localFile);
    solver.exec(ConflictSolver::KeepRemoteVersion);
}

void SocketApi::command_MOVE_ITEM(const QString &localFile, SocketListener *)
{
    const auto fileData = FileData::get(localFile);
    const auto parentDir = fileData.parentFolder();
    if (!fileData.folder)
        return; // should not have shown menu item

    QString defaultDirAndName = fileData.folderRelativePath;

    // If it's a conflict, we want to save it under the base name by default
    if (Utility::isConflictFile(defaultDirAndName)) {
        defaultDirAndName = fileData.folder->journalDb()->conflictFileBaseName(fileData.folderRelativePath.toUtf8());
    }

    // If the parent doesn't accept new files, go to the root of the sync folder
    QFileInfo fileInfo(localFile);
    const auto parentRecord = parentDir.journalRecord();
    if ((fileInfo.isFile() && !parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddFile))
        || (fileInfo.isDir() && !parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddSubDirectories))) {
        defaultDirAndName = QFileInfo(defaultDirAndName).fileName();
    }

    // Add back the folder path
    defaultDirAndName = QDir(fileData.folder->path()).filePath(defaultDirAndName);

    const auto target = QFileDialog::getSaveFileName(
        nullptr,
        tr("Select new location …"),
        defaultDirAndName,
        QString(), nullptr, QFileDialog::HideNameFilterDetails);
    if (target.isEmpty())
        return;

    ConflictSolver solver;
    solver.setLocalVersionFilename(localFile);
    solver.setRemoteVersionFilename(target);
}

void SocketApi::command_V2_LIST_ACCOUNTS(const QSharedPointer<SocketApiJobV2> &job) const
{
    QJsonArray out;
    for (auto acc : AccountManager::instance()->accounts()) {
        // TODO: Use uuid once https://github.com/owncloud/client/pull/8397 is merged
        out << QJsonObject({ { "name", acc->account()->displayName() }, { "id", acc->account()->id() } });
    }
    job->success({ { "accounts", out } });
}

void SocketApi::command_V2_UPLOAD_FILES_FROM(const QSharedPointer<SocketApiJobV2> &job) const
{
    auto uploadJob = new SocketUploadJob(job);
    uploadJob->start();
}

void SocketApi::emailPrivateLink(const QString &link)
{
    Utility::openEmailComposer(
        tr("I shared something with you"),
        link,
        nullptr);
}

void OCC::SocketApi::openPrivateLink(const QString &link)
{
    Utility::openBrowser(link);
}

void SocketApi::command_GET_STRINGS(const QString &argument, SocketListener *listener)
{
    static std::array<std::pair<const char *, QString>, 6> strings { {
        { "SHARE_MENU_TITLE", tr("Share options") },
        { "FILE_ACTIVITY_MENU_TITLE", tr("Activity") },
        { "CONTEXT_MENU_TITLE", Theme::instance()->appNameGUI() },
        { "COPY_PRIVATE_LINK_MENU_TITLE", tr("Copy private link to clipboard") },
        { "EMAIL_PRIVATE_LINK_MENU_TITLE", tr("Send private link by email …") },
        { "CONTEXT_MENU_ICON", APPLICATION_ICON_NAME },
    } };
    listener->sendMessage(QString("GET_STRINGS:BEGIN"));
    for (const auto& key_value : strings) {
        if (argument.isEmpty() || argument == QLatin1String(key_value.first)) {
            listener->sendMessage(QString("STRING:%1:%2").arg(key_value.first, key_value.second));
        }
    }
    listener->sendMessage(QString("GET_STRINGS:END"));
}

void SocketApi::sendSharingContextMenuOptions(const FileData &fileData, SocketListener *listener, bool enabled)
{
    auto record = fileData.journalRecord();
    bool isOnTheServer = record.isValid();
    auto flagString = isOnTheServer && enabled ? QLatin1String("::") : QLatin1String(":d:");

    auto capabilities = fileData.folder->accountState()->account()->capabilities();
    auto theme = Theme::instance();
    if (!capabilities.shareAPI() || !(theme->userGroupSharing() || (theme->linkSharing() && capabilities.sharePublicLink())))
        return;

    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !record._remotePerm.isNull() && !record._remotePerm.hasPermission(RemotePermissions::CanReshare)) {
        listener->sendMessage(QLatin1String("MENU_ITEM:DISABLED:d:") + (!record.isDirectory() ? tr("Resharing this file is not allowed") : tr("Resharing this folder is not allowed")));
    } else {
        listener->sendMessage(QLatin1String("MENU_ITEM:SHARE") + flagString + tr("Share options"));

        // Do we have public links?
        bool publicLinksEnabled = theme->linkSharing() && capabilities.sharePublicLink();

        // Is is possible to create a public link without user choices?
        bool canCreateDefaultPublicLink = publicLinksEnabled
            && !capabilities.sharePublicLinkEnforceExpireDate()
            && !capabilities.sharePublicLinkAskOptionalPassword()
            && !capabilities.sharePublicLinkEnforcePassword();

        if (canCreateDefaultPublicLink) {
            listener->sendMessage(QLatin1String("MENU_ITEM:COPY_PUBLIC_LINK") + flagString + tr("Copy public link"));
        } else if (publicLinksEnabled) {
            listener->sendMessage(QLatin1String("MENU_ITEM:MANAGE_PUBLIC_LINKS") + flagString + tr("Copy public link"));
        }
    }

    listener->sendMessage(QLatin1String("MENU_ITEM:COPY_PRIVATE_LINK") + flagString + tr("Copy internal link"));

    // Disabled: only providing email option for private links would look odd,
    // and the copy option is more general.
    //listener->sendMessage(QLatin1String("MENU_ITEM:EMAIL_PRIVATE_LINK") + flagString + tr("Send private link by email …"));
}

SocketApi::FileData SocketApi::FileData::get(const QString &localFile)
{
    FileData data;

    data.localPath = QDir::cleanPath(localFile);
    if (data.localPath.endsWith(QLatin1Char('/')))
        data.localPath.chop(1);

    data.folder = FolderMan::instance()->folderForPath(data.localPath);
    if (!data.folder)
        return data;

    data.folderRelativePath = data.localPath.mid(data.folder->cleanPath().length() + 1);
    data.serverRelativePath = QDir(data.folder->remotePath()).filePath(data.folderRelativePath);
    QString virtualFileExt = QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
    if (data.serverRelativePath.endsWith(virtualFileExt)) {
        data.serverRelativePath.chop(virtualFileExt.size());
    }
    return data;
}

QString SocketApi::FileData::folderRelativePathNoVfsSuffix() const
{
    auto result = folderRelativePath;
    QString virtualFileExt = QStringLiteral(APPLICATION_DOTVIRTUALFILE_SUFFIX);
    if (result.endsWith(virtualFileExt)) {
        result.chop(virtualFileExt.size());
    }
    return result;
}

SyncFileStatus SocketApi::FileData::syncFileStatus() const
{
    if (!folder)
        return SyncFileStatus::StatusNone;
    return folder->syncEngine().syncFileStatusTracker().fileStatus(folderRelativePath);
}

SyncJournalFileRecord SocketApi::FileData::journalRecord() const
{
    SyncJournalFileRecord record;
    if (!folder)
        return record;
    folder->journalDb()->getFileRecord(folderRelativePath, &record);
    return record;
}

SocketApi::FileData SocketApi::FileData::parentFolder() const
{
    return FileData::get(QFileInfo(localPath).dir().path().toUtf8());
}

void SocketApi::command_GET_MENU_ITEMS(const QString &argument, OCC::SocketListener *listener)
{
    listener->sendMessage(QString("GET_MENU_ITEMS:BEGIN"));
    const QStringList files = split(argument);

    // Find the common sync folder.
    // syncFolder will be null if files are in different folders.
    Folder *syncFolder = nullptr;
    for (const auto &file : files) {
        auto folder = FolderMan::instance()->folderForPath(file);
        if (folder != syncFolder) {
            if (!syncFolder) {
                syncFolder = folder;
            } else {
                syncFolder = nullptr;
                break;
            }
        }
    }

    // Sharing actions show for single files only
    if (syncFolder && files.size() == 1 && syncFolder->accountState()->isConnected()) {
        QString systemPath = QDir::cleanPath(argument);
        if (systemPath.endsWith(QLatin1Char('/'))) {
            systemPath.truncate(systemPath.length() - 1);
        }

        FileData fileData = FileData::get(argument);
        const auto record = fileData.journalRecord();
        const bool isOnTheServer = record.isValid();
        const auto isE2eEncryptedPath = fileData.journalRecord()._isE2eEncrypted || !fileData.journalRecord()._e2eMangledName.isEmpty();
        auto flagString = isOnTheServer && !isE2eEncryptedPath ? QLatin1String("::") : QLatin1String(":d:");

        const QFileInfo fileInfo(fileData.localPath);
        if (!fileInfo.isDir()) {
            listener->sendMessage(QLatin1String("MENU_ITEM:ACTIVITY") + flagString + tr("Activity"));
        }

        DirectEditor* editor = getDirectEditorForLocalFile(fileData.localPath);
        if (editor) {
            //listener->sendMessage(QLatin1String("MENU_ITEM:EDIT") + flagString + tr("Edit via ") + editor->name());
            listener->sendMessage(QLatin1String("MENU_ITEM:EDIT") + flagString + tr("Edit"));
        } else {
            listener->sendMessage(QLatin1String("MENU_ITEM:OPEN_PRIVATE_LINK") + flagString + tr("Open in browser"));
        }

        sendSharingContextMenuOptions(fileData, listener, !isE2eEncryptedPath);

        // Conflict files get conflict resolution actions
        bool isConflict = Utility::isConflictFile(fileData.folderRelativePath);
        if (isConflict || !isOnTheServer) {
            // Check whether this new file is in a read-only directory
            const auto parentDir = fileData.parentFolder();
            const auto parentRecord = parentDir.journalRecord();
            const bool canAddToDir =
                !parentRecord.isValid() // We're likely at the root of the sync folder, got to assume we can add there
                || (fileInfo.isFile() && parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddFile))
                || (fileInfo.isDir() && parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddSubDirectories));
            const bool canChangeFile =
                !isOnTheServer
                || (record._remotePerm.hasPermission(RemotePermissions::CanDelete)
                       && record._remotePerm.hasPermission(RemotePermissions::CanMove)
                       && record._remotePerm.hasPermission(RemotePermissions::CanRename));

            if (isConflict && canChangeFile) {
                if (canAddToDir) {
                    listener->sendMessage(QLatin1String("MENU_ITEM:RESOLVE_CONFLICT::") + tr("Resolve conflict …"));
                } else {
                    if (isOnTheServer) {
                        // Uploaded conflict file in read-only directory
                        listener->sendMessage(QLatin1String("MENU_ITEM:MOVE_ITEM::") + tr("Move and rename …"));
                    } else {
                        // Local-only conflict file in a read-only dir
                        listener->sendMessage(QLatin1String("MENU_ITEM:MOVE_ITEM::") + tr("Move, rename and upload …"));
                    }
                    listener->sendMessage(QLatin1String("MENU_ITEM:DELETE_ITEM::") + tr("Delete local changes"));
                }
            }

            // File in a read-only directory?
            if (!isConflict && !isOnTheServer && !canAddToDir) {
                listener->sendMessage(QLatin1String("MENU_ITEM:MOVE_ITEM::") + tr("Move and upload …"));
                listener->sendMessage(QLatin1String("MENU_ITEM:DELETE_ITEM::") + tr("Delete"));
            }
        }
    }

    // File availability actions
    if (syncFolder
        && syncFolder->virtualFilesEnabled()
        && syncFolder->vfs().socketApiPinStateActionsShown()) {
        ENFORCE(!files.isEmpty());

        // Determine the combined availability status of the files
        auto combined = Optional<VfsItemAvailability>();
        auto merge = [](VfsItemAvailability lhs, VfsItemAvailability rhs) {
            if (lhs == rhs)
                return lhs;
            if (int(lhs) > int(rhs))
                std::swap(lhs, rhs); // reduce cases ensuring lhs < rhs
            if (lhs == VfsItemAvailability::AlwaysLocal && rhs == VfsItemAvailability::AllHydrated)
                return VfsItemAvailability::AllHydrated;
            if (lhs == VfsItemAvailability::AllDehydrated && rhs == VfsItemAvailability::OnlineOnly)
                return VfsItemAvailability::AllDehydrated;
            return VfsItemAvailability::Mixed;
        };
        for (const auto &file : files) {
            auto fileData = FileData::get(file);
            auto availability = syncFolder->vfs().availability(fileData.folderRelativePath);
            if (!availability) {
                if (availability.error() == Vfs::AvailabilityError::DbError)
                    availability = VfsItemAvailability::Mixed;
                if (availability.error() == Vfs::AvailabilityError::NoSuchItem)
                    continue;
            }
            if (!combined) {
                combined = *availability;
            } else {
                combined = merge(*combined, *availability);
            }
        }

        // TODO: Should be a submenu, should use icons
        auto makePinContextMenu = [&](bool makeAvailableLocally, bool freeSpace) {
            listener->sendMessage(QLatin1String("MENU_ITEM:CURRENT_PIN:d:")
                + Utility::vfsCurrentAvailabilityText(*combined));
            if (!Theme::instance()->enforceVirtualFilesSyncFolder()) {
                listener->sendMessage(QLatin1String("MENU_ITEM:MAKE_AVAILABLE_LOCALLY:")
                    + (makeAvailableLocally ? QLatin1String(":") : QLatin1String("d:")) + Utility::vfsPinActionText());
            }
            
            listener->sendMessage(QLatin1String("MENU_ITEM:MAKE_ONLINE_ONLY:")
                + (freeSpace ? QLatin1String(":") : QLatin1String("d:"))
                + Utility::vfsFreeSpaceActionText());
        };

        if (combined) {
            switch (*combined) {
            case VfsItemAvailability::AlwaysLocal:
                makePinContextMenu(false, true);
                break;
            case VfsItemAvailability::AllHydrated:
            case VfsItemAvailability::Mixed:
                makePinContextMenu(true, true);
                break;
            case VfsItemAvailability::AllDehydrated:
            case VfsItemAvailability::OnlineOnly:
                makePinContextMenu(true, false);
                break;
            }
        }
    }

    listener->sendMessage(QString("GET_MENU_ITEMS:END"));
}

DirectEditor* SocketApi::getDirectEditorForLocalFile(const QString &localFile)
{
    FileData fileData = FileData::get(localFile);
    auto capabilities = fileData.folder->accountState()->account()->capabilities();

    if (fileData.folder && fileData.folder->accountState()->isConnected()) {
        const auto record = fileData.journalRecord();
        const auto mimeMatchMode = record.isVirtualFile() ? QMimeDatabase::MatchExtension : QMimeDatabase::MatchDefault;

        QMimeDatabase db;
        QMimeType type = db.mimeTypeForFile(localFile, mimeMatchMode);

        DirectEditor* editor = capabilities.getDirectEditorForMimetype(type);
        if (!editor) {
            editor = capabilities.getDirectEditorForOptionalMimetype(type);
        }
        return editor;
    }

    return nullptr;
}

#if GUI_TESTING
void SocketApi::command_ASYNC_LIST_WIDGETS(const QSharedPointer<SocketApiJob> &job)
{
    QString response;
    for (auto &widget : allObjects(QApplication::allWidgets())) {
        auto objectName = widget->objectName();
        if (!objectName.isEmpty()) {
            response += objectName + ":" + widget->property("text").toString() + ", ";
        }
    }
    job->resolve(response);
}

void SocketApi::command_ASYNC_INVOKE_WIDGET_METHOD(const QSharedPointer<SocketApiJob> &job)
{
    auto &arguments = job->arguments();

    auto widget = findWidget(arguments["objectName"].toString());
    if (!widget) {
        job->reject(QLatin1String("widget not found"));
        return;
    }

    QMetaObject::invokeMethod(widget, arguments["method"].toString().toUtf8().constData());
    job->resolve();
}

void SocketApi::command_ASYNC_GET_WIDGET_PROPERTY(const QSharedPointer<SocketApiJob> &job)
{
    QString widgetName = job->arguments()[QLatin1String("objectName")].toString();
    auto widget = findWidget(widgetName);
    if (!widget) {
        QString message = QString(QLatin1String("Widget not found: 2: %1")).arg(widgetName);
        job->reject(message);
        return;
    }

    auto propertyName = job->arguments()[QLatin1String("property")].toString();

    auto segments = propertyName.split('.');

    QObject *currentObject = widget;
    QString value;
    for (int i = 0; i < segments.count(); i++) {
        auto segment = segments.at(i);
        auto var = currentObject->property(segment.toUtf8().constData());

        if (var.canConvert<QString>()) {
            var.convert(QMetaType::QString);
            value = var.value<QString>();
            break;
        }

        auto tmpObject = var.value<QObject *>();
        if (tmpObject) {
            currentObject = tmpObject;
        } else {
            QString message = QString(QLatin1String("Widget not found: 3: %1")).arg(widgetName);
            job->reject(message);
            return;
        }
    }

    job->resolve(value);
}

void SocketApi::command_ASYNC_SET_WIDGET_PROPERTY(const QSharedPointer<SocketApiJob> &job)
{
    auto &arguments = job->arguments();
    QString widgetName = arguments["objectName"].toString();
    auto widget = findWidget(widgetName);
    if (!widget) {
        QString message = QString(QLatin1String("Widget not found: 4: %1")).arg(widgetName);
        job->reject(message);
        return;
    }
    widget->setProperty(arguments["property"].toString().toUtf8().constData(),
        arguments["value"]);

    job->resolve();
}

void SocketApi::command_ASYNC_WAIT_FOR_WIDGET_SIGNAL(const QSharedPointer<SocketApiJob> &job)
{
    auto &arguments = job->arguments();
    QString widgetName = arguments["objectName"].toString();
    auto widget = findWidget(arguments["objectName"].toString());
    if (!widget) {
        QString message = QString(QLatin1String("Widget not found: 5: %1")).arg(widgetName);
        job->reject(message);
        return;
    }

    ListenerClosure *closure = new ListenerClosure([job]() { job->resolve("signal emitted"); });

    auto signalSignature = arguments["signalSignature"].toString();
    signalSignature.prepend("2");
    auto utf8 = signalSignature.toUtf8();
    auto signalSignatureFinal = utf8.constData();
    connect(widget, signalSignatureFinal, closure, SLOT(closureSlot()), Qt::QueuedConnection);
}

void SocketApi::command_ASYNC_TRIGGER_MENU_ACTION(const QSharedPointer<SocketApiJob> &job)
{
    auto &arguments = job->arguments();

    auto objectName = arguments["objectName"].toString();
    auto widget = findWidget(objectName);
    if (!widget) {
        QString message = QString(QLatin1String("Object not found: 1: %1")).arg(objectName);
        job->reject(message);
        return;
    }

    auto children = widget->findChildren<QWidget *>();
    for (auto childWidget : children) {
        // foo is the popupwidget!
        auto actions = childWidget->actions();
        for (auto action : actions) {
            if (action->objectName() == arguments["actionName"].toString()) {
                action->trigger();

                job->resolve("action found");
                return;
            }
        }
    }

    QString message = QString(QLatin1String("Action not found: 1: %1")).arg(arguments["actionName"].toString());
    job->reject(message);
}

void SocketApi::command_ASYNC_ASSERT_ICON_IS_EQUAL(const QSharedPointer<SocketApiJob> &job)
{
    auto widget = findWidget(job->arguments()[QLatin1String("queryString")].toString());
    if (!widget) {
        QString message = QString(QLatin1String("Object not found: 6: %1")).arg(job->arguments()["queryString"].toString());
        job->reject(message);
        return;
    }

    auto propertyName = job->arguments()[QLatin1String("propertyPath")].toString();

    auto segments = propertyName.split('.');

    QObject *currentObject = widget;
    QIcon value;
    for (int i = 0; i < segments.count(); i++) {
        auto segment = segments.at(i);
        auto var = currentObject->property(segment.toUtf8().constData());

        if (var.canConvert<QIcon>()) {
            var.convert(QMetaType::QIcon);
            value = var.value<QIcon>();
            break;
        }

        auto tmpObject = var.value<QObject *>();
        if (tmpObject) {
            currentObject = tmpObject;
        } else {
            job->reject(QString(QLatin1String("Icon not found: %1")).arg(propertyName));
        }
    }

    auto iconName = job->arguments()[QLatin1String("iconName")].toString();
    if (value.name() == iconName) {
        job->resolve();
    } else {
        job->reject("iconName " + iconName + " does not match: " + value.name());
    }
}
#endif

QString SocketApi::buildRegisterPathMessage(const QString &path)
{
    QFileInfo fi(path);
    QString message = QLatin1String("REGISTER_PATH:");
    message.append(QDir::toNativeSeparators(fi.absoluteFilePath()));
    return message;
}

void SocketApiJob::resolve(const QString &response)
{
    _socketListener->sendMessage(QStringLiteral("RESOLVE|") + _jobId + QLatin1Char('|') + response);
}

void SocketApiJob::resolve(const QJsonObject &response)
{
    resolve(QJsonDocument { response }.toJson());
}

void SocketApiJob::reject(const QString &response)
{
    _socketListener->sendMessage(QStringLiteral("REJECT|") + _jobId + QLatin1Char('|') + response);
}

SocketApiJobV2::SocketApiJobV2(const QSharedPointer<SocketListener> &socketListener, const QByteArray &command, const QJsonObject &arguments)
    : _socketListener(socketListener)
    , _command(command)
    , _jobId(arguments[QStringLiteral("id")].toString())
    , _arguments(arguments[QStringLiteral("arguments")].toObject())
{
    ASSERT(!_jobId.isEmpty())
}

void SocketApiJobV2::success(const QJsonObject &response) const
{
    doFinish(response);
}

void SocketApiJobV2::failure(const QString &error) const
{
    doFinish({ { QStringLiteral("error"), error } });
}

void SocketApiJobV2::doFinish(const QJsonObject &obj) const
{
    _socketListener->sendMessage(_command + QStringLiteral("_RESULT:") + QJsonDocument({ { QStringLiteral("id"), _jobId }, { QStringLiteral("arguments"), obj } }).toJson(QJsonDocument::Compact));
    Q_EMIT finished();
}

} // namespace OCC

#include "socketapi.moc"
