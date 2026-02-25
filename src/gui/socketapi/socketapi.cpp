/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "socketapi.h"
#include "socketapi_p.h"

#include "conflictdialog.h"
#include "conflictsolver.h"

#include "config.h"
#include "configfile.h"
#include "deletejob.h"
#include "folderman.h"
#include "folder.h"
#include "encryptfolderjob.h"
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

#include <QDesktopServices>

#include <QProcess>
#include <QStandardPaths>

#ifdef Q_OS_MACOS
#include <CoreFoundation/CoreFoundation.h>
#include "common/utility_mac_sandbox.h"
#endif

#ifdef HAVE_KGUIADDONS
#include <QMimeData>
#include <KSystemClipboard>
#else
#include <QClipboard>
#endif

// This is the version that is returned when the client asks for the VERSION.
// The first number should be changed if there is an incompatible change that breaks old clients.
// The second number should be changed when there are new features.
#define MIRALL_SOCKET_API_VERSION "1.1"

using namespace Qt::StringLiterals;

namespace {
constexpr auto encryptJobPropertyFolder = "folder";
constexpr auto encryptJobPropertyPath = "path";
}

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

void setClipboardText(const QString &text)
{
#ifdef HAVE_KGUIADDONS
    auto mimeData = new QMimeData();
    mimeData->setText(text);
    KSystemClipboard::instance()->setMimeData(mimeData, QClipboard::Clipboard);
#else
    QApplication::clipboard()->setText(text);
#endif
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
#ifdef Q_OS_MACOS
        socketPath = socketApiSocketPath();
#endif
    } else if (Utility::isLinux() || Utility::isBSD()) {
        QString runtimeDir;
        runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        socketPath = runtimeDir + "/" + Theme::instance()->appName() + "/socket";
    } else {
        qCWarning(lcSocketApi) << "An unexpected system detected, this probably won't work.";
    }

    QLocalServer::removeServer(socketPath);
    // Create the socket path:
    if (!Utility::isMac()) {
        // Not on macOS: there the directory is there, and created for us by the sandboxing
        // environment, because we belong to an App Group.
        QFileInfo info(socketPath);
        if (!info.dir().exists()) {
            bool result = info.dir().mkpath(".");
            qCDebug(lcSocketApi) << "creating" << info.dir().path() << result;
            if (result) {
                QFile::setPermissions(socketPath,
                    QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
            }
        }
    }

    if (!_localServer.listen(socketPath)) {
        qCWarning(lcSocketApi) << "can't start server" 
                               << socketPath
                               << "Error:"
                               << _localServer.errorString()
                               << "Error code:" 
                               << _localServer.serverError();
    } else {
        qCInfo(lcSocketApi) << "server started, listening at " << socketPath;
    }

    connect(&_localServer, &QLocalServer::newConnection, this, &SocketApi::slotNewConnection);

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
    QIODevice *socket = _localServer.nextPendingConnection();

    if (!socket) {
        return;
    }
    qCDebug(lcSocketApi) << "New connection" << socket;
    connect(socket, &QIODevice::readyRead, this, &SocketApi::slotReadSocket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    connect(socket, &QObject::destroyed, this, &SocketApi::slotSocketDestroyed);
    ASSERT(socket->readAll().isEmpty());

    auto listener = QSharedPointer<SocketListener>::create(socket);
    _listeners.insert(socket, listener);
    for (Folder *f : FolderMan::instance()->map()) {
        if (f->canSync()) {
            QString message = buildRegisterPathMessage(removeTrailingSlash(f->path()));
            qCDebug(lcSocketApi) << "Trying to send SocketAPI Register Path Message -->" << message << "to" << listener->socket;
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
    auto *socket = dynamic_cast<QIODevice *>(obj);
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
        qCDebug(lcSocketApi) << "Received SocketAPI message <--" << line << "from" << socket;
        const int argPos = line.indexOf(QLatin1Char(':'));
        const QByteArray command = line.mid(0, argPos).toUtf8().toUpper();
        const int indexOfMethod = [&] {
            QByteArray functionWithArguments = QByteArrayLiteral("command_");
            if (command.startsWith("ASYNC_")) {
                functionWithArguments += command + QByteArrayLiteral("(QSharedPointer<SocketApiJob>)");
            } else {
                functionWithArguments += command + QByteArrayLiteral("(QString,SocketListener*)");
            }
            Q_ASSERT(staticMetaObject.normalizedSignature(functionWithArguments) == functionWithArguments);
            const auto out = staticMetaObject.indexOfMethod(functionWithArguments);
            if (out == -1) {
                listener->sendError(QStringLiteral("Function %1 not found").arg(QString::fromUtf8(functionWithArguments)));
            }
            return out;
        }();

        const auto argument = QString{argPos != -1 ? line.mid(argPos + 1) : QString()};
        if (command.startsWith("ASYNC_"_L1)) {
            const auto arguments = argument.split('|');
            if (arguments.size() != 2) {
                listener->sendError(QStringLiteral("argument count is wrong"));
                return;
            }

            auto json = QJsonDocument::fromJson(arguments[1].toUtf8()).object();

            auto jobId = arguments[0];

            auto socketApiJob = QSharedPointer<SocketApiJob>(
                new SocketApiJob(jobId, listener, json), &QObject::deleteLater);
            if (indexOfMethod != -1) {
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::QueuedConnection,
                        Q_ARG(QSharedPointer<SocketApiJob>, socketApiJob));
            } else {
                qCWarning(lcSocketApi) << "The command is not supported by this version of the client:" << command
                                       << "with argument:" << argument;
                socketApiJob->reject(QStringLiteral("command not found"));
            }
        } else if (command.startsWith("ENCRYPT")) {
            if (indexOfMethod != -1) {
                ASSERT(thread() == QThread::currentThread())
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::QueuedConnection, Q_ARG(QString, argument),
                            Q_ARG(SocketListener *, listener.data()));
            }
        } else {
            if (indexOfMethod != -1) {
                // to ensure that listener is still valid we need to call it with Qt::DirectConnection
                ASSERT(thread() == QThread::currentThread())
                staticMetaObject.method(indexOfMethod)
                    .invoke(this, Qt::DirectConnection, Q_ARG(QString, argument),
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
        for (const auto &listener : std::as_const(_listeners)) {
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
    for (const auto &listener : std::as_const(_listeners)) {
        listener->sendMessage(msg, doWait);
    }
}

void SocketApi::processFileActivityRequest(const QString &localFile)
{
    const auto fileData = FileData::get(localFile);
    emit fileActivityCommandReceived(fileData.localPath);
}

void SocketApi::processEncryptRequest(const QString &localFile)
{
    Q_ASSERT(QFileInfo(localFile).isDir());

    const auto fileData = FileData::get(localFile);

    const auto folder = fileData.folder;
    Q_ASSERT(folder);

    const auto account = folder->accountState()->account();
    Q_ASSERT(account);

    const auto rec = fileData.journalRecord();
    Q_ASSERT(rec.isValid());

    if (!account->e2e() || !account->e2e()->isInitialized()) {
        const int ret = QMessageBox::critical(
            nullptr,
            tr("Failed to encrypt folder at \"%1\"").arg(fileData.folderRelativePath),
            tr("The account %1 does not have end-to-end encryption configured. "
               "Please configure this in your account settings to enable folder encryption.").arg(account->prettyName()),
            QMessageBox::Ok
            );
        Q_UNUSED(ret)
        return;
    }

    auto path = rec._path;
    // Folder records have directory paths in Foo/Bar/ convention...
    // But EncryptFolderJob expects directory path Foo/Bar convention
    const auto choppedPath = Utility::noTrailingSlashPath(Utility::noLeadingSlashPath(path));

    auto job = new OCC::EncryptFolderJob(account, folder->journalDb(), choppedPath, choppedPath, folder->remotePath(), rec.numericFileId());
    job->setParent(this);
    connect(job, &OCC::EncryptFolderJob::finished, this, [fileData, job](const int status) {
        if (status == OCC::EncryptFolderJob::Error) {
            const int ret = QMessageBox::critical(
                nullptr,
                tr("Failed to encrypt folder"),
                tr("Could not encrypt the following folder: \"%1\".\n\n"
                   "Server replied with error: %2").arg(fileData.folderRelativePath, job->errorString()),
                QMessageBox::Ok
            );
            Q_UNUSED(ret)
        } else {
            const int ret = QMessageBox::information(nullptr,
                                                     tr("Folder encrypted successfully").arg(fileData.folderRelativePath),
                                                     tr("The following folder was encrypted successfully: \"%1\"").arg(fileData.folderRelativePath));
            Q_UNUSED(ret)
        }
    });
    job->setProperty(encryptJobPropertyFolder, QVariant::fromValue(folder));
    job->setProperty(encryptJobPropertyPath, QVariant::fromValue(path));
    job->start();
}

void SocketApi::processShareRequest(const QString &localFile, SocketListener *listener)
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

        if (!fileData.journalRecord().e2eMangledName().isEmpty()) {
            // we can not share an encrypted file or a subfolder under encrypted root foolder
            const QString message = QLatin1String("SHARE:NOP:") + QDir::toNativeSeparators(localFile);
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

        emit shareCommandReceived(fileData.localPath);
    }
}

void SocketApi::processLeaveShareRequest(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener)
    FolderMan::instance()->leaveShare(QDir::fromNativeSeparators(localFile));
}

void SocketApi::processFileActionsRequest(const QString &localFile)
{
    const auto fileData = FileData::get(localFile);
    emit fileActionsCommandReceived(fileData.localPath);
}

void SocketApi::broadcastStatusPushMessage(const QString &systemPath, SyncFileStatus fileStatus)
{
    QString msg = buildMessage(QLatin1String("STATUS"), systemPath, fileStatus.toSocketAPIString());
    Q_ASSERT(!systemPath.endsWith('/'));
    uint directoryHash = qHash(systemPath.left(systemPath.lastIndexOf('/')));
    for (const auto &listener : std::as_const(_listeners)) {
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
    processShareRequest(localFile, listener);
}

void SocketApi::command_LEAVESHARE(const QString &localFile, SocketListener *listener)
{
    processLeaveShareRequest(localFile, listener);
}

void SocketApi::command_ACTIVITY(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener);

    processFileActivityRequest(localFile);
}

void SocketApi::command_ENCRYPT(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener);

    processEncryptRequest(localFile);
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
        auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toObject();
        auto url = QUrl(data.value("url"_L1).toString());

        if(!url.isEmpty())
            Utility::openBrowser(url);
    });
    job->start();
}

void SocketApi::command_FILE_ACTIONS(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener);

    processFileActionsRequest(localFile);
}

// don't pull the share manager into socketapi unittests
#ifndef OWNCLOUD_TEST

class GetOrCreatePublicLinkShare : public QObject
{
    Q_OBJECT
public:
    GetOrCreatePublicLinkShare(const AccountPtr &account, const QString &localFile, const bool isSecureFileDropOnlyFolder,
        QObject *parent)
        : QObject(parent)
        , _account(account)
        , _shareManager(account)
        , _localFile(localFile)
        , _isSecureFileDropOnlyFolder(isSecureFileDropOnlyFolder)
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
    void sharesFetched(const QList<OCC::SharePtr> &shares)
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
        if (_isSecureFileDropOnlyFolder) {
            _shareManager.createSecureFileDropShare(_localFile, shareName, QString());
        } else {
            _shareManager.createLinkShare(_localFile, shareName, QString());
        }
    }

    void linkShareCreated(const QSharedPointer<OCC::LinkShare> &share)
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
    bool _isSecureFileDropOnlyFolder = false;
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

void SocketApi::command_COPY_SECUREFILEDROP_LINK(const QString &localFile, SocketListener *)
{
    const auto fileData = FileData::get(localFile);
    if (!fileData.folder) {
        return;
    }

    const auto account = fileData.folder->accountState()->account();
    const auto getOrCreatePublicLinkShareJob = new GetOrCreatePublicLinkShare(account, fileData.serverRelativePath, true, this);
    connect(getOrCreatePublicLinkShareJob, &GetOrCreatePublicLinkShare::done, this, [](const QString &url) { copyUrlToClipboard(url); });
    connect(getOrCreatePublicLinkShareJob, &GetOrCreatePublicLinkShare::error, this, [=, this]() { emit shareCommandReceived(fileData.localPath); });
    getOrCreatePublicLinkShareJob->run();
}

// Windows Shell / Explorer pinning fallbacks, see issue: https://github.com/nextcloud/desktop/issues/1599
#ifdef Q_OS_WIN
void SocketApi::command_COPYASPATH(const QString &localFile, SocketListener *)
{
    setClipboardText(localFile);
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
    setClipboardText(link);
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
    if ((FileSystem::isFile(localFile) && !parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddFile))
        || (FileSystem::isDir(localFile) && !parentRecord._remotePerm.hasPermission(RemotePermissions::CanAddSubDirectories))) {
        defaultDirAndName = QFileInfo(defaultDirAndName).fileName();
    }

    // Add back the folder path
    defaultDirAndName = QDir(fileData.folder->path()).filePath(defaultDirAndName);

    // Use getSaveFileUrl for sandbox compatibility
    const auto targetUrl = QFileDialog::getSaveFileUrl(
        nullptr,
        tr("Select new location …"),
        QUrl::fromLocalFile(defaultDirAndName),
        QString(), nullptr, QFileDialog::HideNameFilterDetails);
    if (targetUrl.isEmpty())
        return;

#ifdef Q_OS_MACOS
    // On macOS with app sandbox, we need to explicitly access the security-scoped resource
    auto scopedAccess = Utility::MacSandboxSecurityScopedAccess::create(targetUrl);
    
    if (!scopedAccess->isValid()) {
        qCWarning(lcSocketApi) << "Could not access security-scoped resource for conflict resolution:" << targetUrl;
        return;
    }
#endif

    const auto target = targetUrl.toLocalFile();

    ConflictSolver solver;
    solver.setLocalVersionFilename(localFile);
    solver.setRemoteVersionFilename(target);
}

void SocketApi::command_LOCK_FILE(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener)

    setFileLock(localFile, SyncFileItem::LockStatus::LockedItem);
}

void SocketApi::command_UNLOCK_FILE(const QString &localFile, SocketListener *listener)
{
    Q_UNUSED(listener)

    setFileLock(localFile, SyncFileItem::LockStatus::UnlockedItem);
}

void SocketApi::setFileLock(const QString &localFile, const SyncFileItem::LockStatus lockState) const
{
    const auto fileData = FileData::get(localFile);

    const auto shareFolder = fileData.folder;
    if (!shareFolder || !shareFolder->accountState()->isConnected()) {
        return;
    }

    const auto record = fileData.journalRecord();

    if (lockState == SyncFileItem::LockStatus::UnlockedItem &&
        !shareFolder->accountState()->account()->fileCanBeUnlocked(shareFolder->journalDb(), fileData.folderRelativePath)) {
        return;
    }

    shareFolder->accountState()->account()->setLockFileState(fileData.serverRelativePath,
                                                             shareFolder->remotePathTrailingSlash(),
                                                             shareFolder->path(),
                                                             record._etag,
                                                             shareFolder->journalDb(),
                                                             lockState,
                                                             (lockState == SyncFileItem::LockStatus::UnlockedItem) ? static_cast<SyncFileItem::LockOwnerType>(record._lockstate._lockOwnerType) : SyncFileItem::LockOwnerType::UserLock);

    shareFolder->journalDb()->schedulePathForRemoteDiscovery(fileData.serverRelativePath);
    shareFolder->scheduleThisFolderSoon();
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
    static std::array<std::pair<const char *, QString>, 7> strings { {
        { "SHARE_MENU_TITLE", tr("Share options") },
        { "FILE_ACTIONS_MENU_TITLE", tr("File actions") },
        { "FILE_ACTIVITY_MENU_TITLE", tr("Activity") },
        { "CONTEXT_MENU_TITLE", Theme::instance()->appNameGUI() },
        { "COPY_PRIVATE_LINK_MENU_TITLE", tr("Copy private link to clipboard") },
        { "EMAIL_PRIVATE_LINK_MENU_TITLE", tr("Send private link by email …") },
        { "CONTEXT_MENU_ICON", APPLICATION_ICON_NAME },
    } };
    listener->sendMessage(QStringLiteral("GET_STRINGS:BEGIN"));
    for (const auto& key_value : strings) {
        if (argument.isEmpty() || argument == QLatin1String(key_value.first)) {
            listener->sendMessage(QStringLiteral("STRING:%1:%2").arg(key_value.first, key_value.second));
        }
    }
    listener->sendMessage(QStringLiteral("GET_STRINGS:END"));
}

void SocketApi::sendSharingContextMenuOptions(const FileData &fileData, SocketListener *listener, SharingContextItemEncryptedFlag itemEncryptionFlag, SharingContextItemRootEncryptedFolderFlag rootE2eeFolderFlag)
{
    const auto record = fileData.journalRecord();
    const auto isOnTheServer = record.isValid();
    const auto isSecureFileDropSupported = rootE2eeFolderFlag == SharingContextItemRootEncryptedFolderFlag::RootEncryptedFolder && fileData.folder->accountState()->account()->secureFileDropSupported();
    const auto flagString = isOnTheServer && (itemEncryptionFlag == SharingContextItemEncryptedFlag::NotEncryptedItem || isSecureFileDropSupported) ? QLatin1String("::") : QLatin1String(":d:");

    auto capabilities = fileData.folder->accountState()->account()->capabilities();
    auto theme = Theme::instance();
    if (!capabilities.shareAPI() || !(theme->userGroupSharing() || (theme->linkSharing() && capabilities.sharePublicLink())))
        return;

    if (record._isShared && !record._sharedByMe) {
        listener->sendMessage(QLatin1String("MENU_ITEM:LEAVESHARE") + flagString + tr("Leave this share"));
    }

    // If sharing is globally disabled, do not show any sharing entries.
    // If there is no permission to share for this file, add a disabled entry saying so
    if (isOnTheServer && !record._remotePerm.isNull() && !record._remotePerm.hasPermission(RemotePermissions::CanReshare)) {
        listener->sendMessage(QLatin1String("MENU_ITEM:DISABLED:d:") + (!record.isDirectory() ? tr("Resharing this file is not allowed") : tr("Resharing this folder is not allowed")));
    } else {
        listener->sendMessage(QLatin1String("MENU_ITEM:SHARE") + flagString + tr("Share options"));
    }

    if (itemEncryptionFlag == SharingContextItemEncryptedFlag::NotEncryptedItem) {
        listener->sendMessage(QLatin1String("MENU_ITEM:COPY_PRIVATE_LINK") + flagString + tr("Copy internal link"));
    }

    // Disabled: only providing email option for private links would look odd,
    // and the copy option is more general.
    //listener->sendMessage(QLatin1String("MENU_ITEM:EMAIL_PRIVATE_LINK") + flagString + tr("Send private link by email …"));
}

void SocketApi::sendFileActionsContextMenuOptions(const FileData &fileData, SocketListener *listener)
{
    const auto record = fileData.journalRecord();
    const auto isOnTheServer = record.isValid();
    auto serverHasIntegration = false;
    if (const auto folder = fileData.folder;folder) {
        if (const auto accountState = folder->accountState();
            accountState && accountState->account()) {
            serverHasIntegration = accountState->account()->serverHasIntegration();
        }
    }

    const auto flagString = isOnTheServer && serverHasIntegration ? QLatin1String("::")
                                                                  : QLatin1String(":d:");
    listener->sendMessage(QLatin1String("MENU_ITEM:FILE_ACTIONS") + flagString + tr("File actions"));
}

void SocketApi::sendEncryptFolderCommandMenuEntries(const QFileInfo &fileInfo,
                                                    const FileData &fileData,
                                                    const bool isE2eEncryptedPath,
                                                    const OCC::SocketListener* const listener) const
{
    if (!listener ||
            !fileData.folder ||
            !fileData.folder->accountState() ||
            !fileData.folder->accountState()->account() ||
            !fileData.folder->accountState()->account()->capabilities().clientSideEncryptionAvailable() ||
            !FileSystem::isDir(fileInfo.absoluteFilePath()) ||
            isE2eEncryptedPath ||
            !fileData.isFolderEmpty()) {
        return;
    }

    bool anyAncestorEncrypted = false;
    auto ancestor = fileData.parentFolder();
    while (ancestor.journalRecord().isValid()) {
        if (ancestor.journalRecord().isE2eEncrypted()) {
            anyAncestorEncrypted = true;
            break;
        }

        ancestor = ancestor.parentFolder();
    }

    if (!anyAncestorEncrypted && !fileData.parentFolder().journalRecord().isValid()) {
        const auto isOnTheServer = fileData.journalRecord().isValid();
        const auto flagString = isOnTheServer ? QLatin1String("::") : QLatin1String(":d:");
        listener->sendMessage(QStringLiteral("MENU_ITEM:ENCRYPT") + flagString + tr("Encrypt"));
    }
}

void SocketApi::sendLockFileCommandMenuEntries(const QFileInfo &fileInfo,
                                               Folder* const syncFolder,
                                               const FileData &fileData,
                                               const OCC::SocketListener* const listener) const
{
    if (!FileSystem::isDir(fileInfo.absoluteFilePath()) && syncFolder->accountState()->account()->capabilities().filesLockAvailable()) {
        if (syncFolder->accountState()->account()->fileLockStatus(syncFolder->journalDb(), fileData.folderRelativePath) == SyncFileItem::LockStatus::UnlockedItem) {
            listener->sendMessage(QLatin1String("MENU_ITEM:LOCK_FILE::") + tr("Lock file"));
        } else {
            if (syncFolder->accountState()->account()->fileCanBeUnlocked(syncFolder->journalDb(), fileData.folderRelativePath)) {
                listener->sendMessage(QLatin1String("MENU_ITEM:UNLOCK_FILE::") + tr("Unlock file"));
            }
        }
    }
}

void SocketApi::sendLockFileInfoMenuEntries(const QFileInfo &fileInfo,
                                            Folder * const syncFolder,
                                            const FileData &fileData,
                                            const SocketListener * const listener,
                                            const SyncJournalFileRecord &record) const
{
    static constexpr auto SECONDS_PER_MINUTE = 60;
    if (!FileSystem::isDir(fileInfo.absoluteFilePath()) && syncFolder->accountState()->account()->capabilities().filesLockAvailable() &&
            syncFolder->accountState()->account()->fileLockStatus(syncFolder->journalDb(), fileData.folderRelativePath) == SyncFileItem::LockStatus::LockedItem) {
        listener->sendMessage(QLatin1String("MENU_ITEM:LOCKED_FILE_OWNER:d:") + tr("Locked by %1").arg(record._lockstate._lockOwnerDisplayName));
        const auto lockExpirationTime = record._lockstate._lockTime + record._lockstate._lockTimeout;
        const auto remainingTime = QDateTime::currentDateTime().secsTo(QDateTime::fromSecsSinceEpoch(lockExpirationTime));
        const auto remainingTimeInMinute = static_cast<int>(remainingTime > 0 ? remainingTime / SECONDS_PER_MINUTE : 0);
        listener->sendMessage(QLatin1String("MENU_ITEM:LOCKED_FILE_DATE:d:") + tr("Expires in %1 minutes", "remaining time before lock expires", remainingTimeInMinute).arg(remainingTimeInMinute));
    }
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

bool SocketApi::FileData::isFolderEmpty() const
{
    if (FileSystem::isDir(localPath)) {
        const auto nativeFolder = QDir{localPath};
        return nativeFolder.isEmpty();
    }
    return false;
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
    if (!folder->journalDb()->getFileRecord(folderRelativePath, &record)) {
        qCWarning(lcSocketApi) << "Failed to get journal record for path" << folderRelativePath;
    }
    return record;
}

SocketApi::FileData SocketApi::FileData::parentFolder() const
{
    return FileData::get(QFileInfo(localPath).dir().path().toUtf8());
}

void SocketApi::command_GET_MENU_ITEMS(const QString &argument, OCC::SocketListener *listener)
{
    listener->sendMessage(QStringLiteral("GET_MENU_ITEMS:BEGIN"), true);
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
        const auto isE2eEncryptedPath = fileData.journalRecord().isE2eEncrypted() || !fileData.journalRecord()._e2eMangledName.isEmpty();
        const auto isE2eEncryptedRootFolder = fileData.journalRecord().isE2eEncrypted() && fileData.journalRecord()._e2eMangledName.isEmpty();
        auto flagString = isOnTheServer && !isE2eEncryptedPath ? QLatin1String("::") : QLatin1String(":d:");

        const QFileInfo fileInfo(fileData.localPath);
        sendLockFileInfoMenuEntries(fileInfo, syncFolder, fileData, listener, record);

        if (!FileSystem::isDir(fileData.localPath)) {
            listener->sendMessage(QLatin1String("MENU_ITEM:ACTIVITY") + flagString + tr("Activity"));
        }

        DirectEditor* editor = getDirectEditorForLocalFile(fileData.localPath);
        if (editor) {
            //listener->sendMessage(QLatin1String("MENU_ITEM:EDIT") + flagString + tr("Edit via ") + editor->name());
            listener->sendMessage(QLatin1String("MENU_ITEM:EDIT") + flagString + tr("Open in browser"));
        } else {
            listener->sendMessage(QLatin1String("MENU_ITEM:OPEN_PRIVATE_LINK") + flagString + tr("Open in browser"));
        }

        sendEncryptFolderCommandMenuEntries(fileInfo, fileData, isE2eEncryptedPath, listener);
        sendLockFileCommandMenuEntries(fileInfo, syncFolder, fileData, listener);
        const auto itemEncryptionFlag = isE2eEncryptedPath ? SharingContextItemEncryptedFlag::EncryptedItem : SharingContextItemEncryptedFlag::NotEncryptedItem;
        const auto rootE2eeFolderFlag = isE2eEncryptedRootFolder ? SharingContextItemRootEncryptedFolderFlag::RootEncryptedFolder : SharingContextItemRootEncryptedFolderFlag::NonRootEncryptedFolder;
        sendSharingContextMenuOptions(fileData, listener, itemEncryptionFlag, rootE2eeFolderFlag);
        sendFileActionsContextMenuOptions(fileData, listener);

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
            auto availability = syncFolder->vfs().availability(fileData.folderRelativePath, Vfs::AvailabilityRecursivity::NotRecursiveAvailability);
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

    listener->sendMessage(QStringLiteral("GET_MENU_ITEMS:END"));
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

    auto closure = new ListenerClosure([job]() { job->resolve("signal emitted"); });

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

} // namespace OCC

#include "socketapi.moc"
