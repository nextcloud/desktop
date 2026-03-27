/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "accountmanager.h"
#include "systray.h"
#include "theme.h"
#include "config.h"
#include "common/utility.h"
#include "tray/svgimageprovider.h"
#include "tray/usermodel.h"
#include "wheelhandler.h"
#include "tray/trayimageprovider.h"
#include "configfile.h"
#include "accessmanager.h"
#include "callstatechecker.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QVariantMap>
#include <QScreen>
#include <QMenu>
#include <QGuiApplication>

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#define NOTIFICATIONS_SERVICE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"
#define NOTIFICATIONS_IFACE "org.freedesktop.Notifications"
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcSystray, "nextcloud.gui.systray")

Systray *Systray::_instance = nullptr;

Systray *Systray::instance()
{
    if (!_instance) {
        _instance = new Systray();
    }
    return _instance;
}

QQmlApplicationEngine *Systray::trayEngine() const
{
    return _trayEngine.get();
}

void Systray::setTrayEngine(QQmlApplicationEngine *trayEngine)
{
    _trayEngine = std::make_unique<QQmlApplicationEngine>(trayEngine);

    _trayEngine->setNetworkAccessManagerFactory(&_accessManagerFactory);

    _trayEngine->addImportPath(QCoreApplication::applicationDirPath() + "/qml");
    _trayEngine->addImportPath("qrc:/qml/theme");
    _trayEngine->addImageProvider("avatars", new ImageProvider);
    _trayEngine->addImageProvider(QLatin1String("svgimage-custom-color"), new OCC::Ui::SvgImageProvider);
    _trayEngine->addImageProvider(QLatin1String("tray-image-provider"), new TrayImageProvider);
}

Systray::Systray()
    : QSystemTrayIcon(nullptr)
{
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    setUserNotificationCenterDelegate();
    checkNotificationAuth(MacNotificationAuthorizationOptions::Default); // No provisional auth, ask user explicitly first time
    registerNotificationCategories(QString(tr("Download")));
#elif !defined(Q_OS_MACOS)
    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Systray::setupContextMenu);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &Systray::setupContextMenu);
    setupContextMenu();
#endif

    connect(UserModel::instance(), &UserModel::currentUserChanged,
        this, &Systray::slotCurrentUserChanged);
    connect(UserModel::instance(), &UserModel::addAccount,
            this, &Systray::openAccountWizard);

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, [this]{ showWindow(); });
#else
    // Since the positioning of the QSystemTrayIcon is borked on non-Windows and non-macOS desktop environments,
    // we hardcode the position of the tray to be in the center when we add a new account from somewhere like
    // the wizard. Otherwise with the conventional method we end up with the tray appearing wherever the cursor
    // is placed

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, [this]{ showWindow(WindowPosition::Center); });
#endif

    connect(FolderMan::instance(), &FolderMan::folderListChanged, this, &Systray::slotSyncFoldersChanged);
    slotSyncFoldersChanged(FolderMan::instance()->map());
}

void Systray::create()
{
    if (_trayEngine) {
        if (!AccountManager::instance()->accounts().isEmpty()) {
            _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
        } else {
            _trayEngine->rootContext()->setContextProperty("activityModel", &_fakeActivityModel);
        }

        QQmlComponent trayWindowComponent(trayEngine(), QStringLiteral("qrc:/qml/src/gui/tray/MainWindow.qml"));

        if(trayWindowComponent.isError()) {
            qCWarning(lcSystray) << trayWindowComponent.errorString();
        } else {
            _trayWindow.reset(qobject_cast<QQuickWindow*>(trayWindowComponent.create()));
        }
    }
    hideWindow();
    emit activated(QSystemTrayIcon::ActivationReason::Unknown);
    slotUpdateSyncPausedState();
    connect(FolderMan::instance(), &FolderMan::folderListChanged, this, &Systray::slotUpdateSyncPausedState);
}

void Systray::showWindow(WindowPosition position)
{
    if(isOpen() || !_trayWindow) {
        return;
    }

    if(position == WindowPosition::Center) {
        positionWindowAtScreenCenter(_trayWindow.data());
    } else {
        positionWindowAtTray(_trayWindow.data());
    }
    _trayWindow->show();
    _trayWindow->raise();
    _trayWindow->requestActivate();

    setIsOpen(true);

    UserModel::instance()->fetchCurrentActivityModel();
}

void Systray::hideWindow()
{
    if(!isOpen() || !_trayWindow) {
        return;
    }

    _trayWindow->hide();
    setIsOpen(false);
}

void Systray::setupContextMenu()
{
    const auto oldContextMenu = _contextMenu.data();
    // If we delete the old QMenu before setting the new one the client will crash on GNOME.
    // Let's delete it once this method is over
    if(oldContextMenu) {
        oldContextMenu->deleteLater();
    }

    _contextMenu = new QMenu();
    // NOTE: for reasons unclear, setting the the new menu after adding all the actions
    // will not work on GNOME, as the old menu will not be correctly replaced.
    setContextMenu(_contextMenu);

    if (AccountManager::instance()->accounts().isEmpty()) {
        _contextMenu->addAction(tr("Add account"), this, &Systray::openAccountWizard);
    } else {
        _contextMenu->addAction(tr("Open %1 Desktop", "Open Nextcloud main window. Placeholer will be the application name. Please keep it.").arg(APPLICATION_NAME), this, [this]{ showWindow(); });
    }

    auto pauseAction = _contextMenu->addAction(tr("Pause sync"), this, &Systray::slotPauseAllFolders);
    auto resumeAction = _contextMenu->addAction(tr("Resume sync"), this, &Systray::slotUnpauseAllFolders);
    _contextMenu->addAction(tr("Settings"), this, &Systray::openSettings);
    _contextMenu->addAction(tr("Help"), this, &Systray::openHelp);
    _contextMenu->addAction(tr("Exit %1").arg(Theme::instance()->appNameGUI()), this, &Systray::shutdown);

    connect(_contextMenu, &QMenu::aboutToShow, [=] {
        const auto folders = FolderMan::instance()->map();

        const auto allPaused = std::all_of(std::cbegin(folders), std::cend(folders), [](Folder *f) { return f->syncPaused(); });
        const auto pauseText = folders.size() > 1 ? tr("Pause sync for all") : tr("Pause sync");
        pauseAction->setText(pauseText);
        pauseAction->setVisible(!allPaused);
        pauseAction->setEnabled(!allPaused);

        const auto anyPaused = std::any_of(std::cbegin(folders), std::cend(folders), [](Folder *f) { return f->syncPaused(); });
        const auto resumeText = folders.size() > 1 ? tr("Resume sync for all") : tr("Resume sync");
        resumeAction->setText(resumeText);
        resumeAction->setVisible(anyPaused);
        resumeAction->setEnabled(anyPaused);
    });
}

void Systray::destroyDialog(QQuickWindow *dialog) const
{
    dialog->destroy();
    dialog->deleteLater();
}

void Systray::createCallDialog(const Activity &callNotification, const AccountStatePtr accountState)
{
    qCDebug(lcSystray) << "Starting a new call dialog for notification with id: " << callNotification._id << "with text: " << callNotification._subject;

    if (_trayEngine && !_callsAlreadyNotified.contains(callNotification._id)) {
        const QVariantMap talkNotificationData{
            {"conversationToken", callNotification._talkNotificationData.conversationToken},
            {"messageId", callNotification._talkNotificationData.messageId},
            {"messageSent", callNotification._talkNotificationData.messageSent},
            {"userAvatar", callNotification._talkNotificationData.userAvatar},
        };

        QVariantList links;
        for(const auto &link : callNotification._links) {
            links.append(QVariantMap{
                {"imageSource", link._imageSource},
                {"imageSourceHovered", link._imageSourceHovered},
                {"label", link._label},
                {"link", link._link},
                {"primary", link._primary},
                {"verb", link._verb},
            });
        }

        const QVariantMap initialProperties{
            {"accountState", QVariant::fromValue(accountState.data())},
            {"talkNotificationData", talkNotificationData},
            {"links", links},
            {"subject", callNotification._subject},
            {"link", callNotification._link},
        };

        const auto callDialog = new QQmlComponent(trayEngine(), QStringLiteral("qrc:/qml/src/gui/tray/CallNotificationDialog.qml"));

        if(callDialog->isError()) {
            qCWarning(lcSystray) << callDialog->errorString();
            return;
        }

        // This call dialog gets deallocated on close conditions
        // by a call from the QML side to the destroyDialog slot
        callDialog->createWithInitialProperties(initialProperties);
        _callsAlreadyNotified.insert(callNotification._id);
    }
}

void Systray::createEditFileLocallyLoadingDialog(const QString &fileName)
{
    if (_editFileLocallyLoadingDialog) {
        return;
    }

    qCDebug(lcSystray) << "Opening a file local editing dialog...";

    const auto editFileLocallyLoadingDialog = new QQmlComponent(trayEngine(), QStringLiteral("qrc:/qml/src/gui/tray/EditFileLocallyLoadingDialog.qml"));

    if (editFileLocallyLoadingDialog->isError()) {
        qCWarning(lcSystray) << editFileLocallyLoadingDialog->errorString();
        return;
    }

    _editFileLocallyLoadingDialog = editFileLocallyLoadingDialog->createWithInitialProperties(QVariantMap{{QStringLiteral("fileName"), fileName}});
}

void Systray::destroyEditFileLocallyLoadingDialog()
{
    if (!_editFileLocallyLoadingDialog) {
        return;
    }
    qCDebug(lcSystray) << "Closing a file local editing dialog...";
    _editFileLocallyLoadingDialog->deleteLater();
    _editFileLocallyLoadingDialog = nullptr;
}

void Systray::createResolveConflictsDialog(const OCC::ActivityList &allConflicts)
{
    const auto conflictsDialog = std::make_unique<QQmlComponent>(trayEngine(), QStringLiteral("qrc:/qml/src/gui/ResolveConflictsDialog.qml"));
    const QVariantMap initialProperties{
                                        {"allConflicts", QVariant::fromValue(allConflicts)},
    };

    if(conflictsDialog->isError()) {
        qCWarning(lcSystray) << conflictsDialog->errorString();
        return;
    }

    // This call dialog gets deallocated on close conditions
    // by a call from the QML side to the destroyDialog slot
    auto dialog = std::unique_ptr<QObject>(conflictsDialog->createWithInitialProperties(initialProperties));
    if (!dialog) {
        return;
    }
    dialog->setParent(QGuiApplication::instance());

    auto dialogWindow = qobject_cast<QQuickWindow*>(dialog.release());
    if (!dialogWindow) {
        return;
    }
    dialogWindow->show();
    dialogWindow->raise();
    dialogWindow->requestActivate();
}

void Systray::createEncryptionTokenDiscoveryDialog()
{
    if (_encryptionTokenDiscoveryDialog) {
        return;
    }

    qCDebug(lcSystray) << "Opening an encryption token discovery dialog...";

    const auto encryptionTokenDiscoveryDialog = new QQmlComponent(_trayEngine.get(), QStringLiteral("qrc:/qml/src/gui/tray/EncryptionTokenDiscoveryDialog.qml"));

    if (encryptionTokenDiscoveryDialog->isError()) {
        qCWarning(lcSystray) << encryptionTokenDiscoveryDialog->errorString();
        return;
    }

    _encryptionTokenDiscoveryDialog = encryptionTokenDiscoveryDialog->createWithInitialProperties(QVariantMap{});
}

void Systray::destroyEncryptionTokenDiscoveryDialog()
{
    if (!_encryptionTokenDiscoveryDialog) {
        return;
    }
    qCDebug(lcSystray) << "Closing an encryption token discovery dialog...";
    _encryptionTokenDiscoveryDialog->deleteLater();
    _encryptionTokenDiscoveryDialog = nullptr;
}

bool Systray::raiseDialogs()
{
    return raiseFileDetailDialogs();
}

bool Systray::raiseFileDetailDialogs(const QString &localPath)
{
    if(_fileDetailDialogs.empty()) {
        return false;
    }

    auto it = _fileDetailDialogs.begin();
    while (it != _fileDetailDialogs.end()) {
        const auto dialog = *it;
        auto nullDialog = dialog == nullptr;

        if (!nullDialog && !dialog->isVisible()) {
            destroyDialog(dialog);
            nullDialog = true;
        }

        if (!nullDialog && (localPath.isEmpty() || dialog->property("localPath").toString() == localPath)) {
            dialog->show();
            dialog->raise();
            dialog->requestActivate();

            ++it;
            continue;
        }

        it = _fileDetailDialogs.erase(it);
        continue;
    }

    // If it is empty then we have raised no dialogs, so return false (and vice-versa)
    return !_fileDetailDialogs.empty();
}

void Systray::createFileDetailsDialog(const QString &localPath)
{
    if (raiseFileDetailDialogs(localPath)) {
        qCDebug(lcSystray) << "Reopening an existing file details dialog for " << localPath;
        return;
    }

    qCDebug(lcSystray) << "Opening new file details dialog for " << localPath;

    if (!_trayEngine) {
        qCWarning(lcSystray) << "Could not open file details dialog for" << localPath << "as no tray engine was available";
        return;
    }

    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        qCWarning(lcSystray) << "Could not open file details dialog for" << localPath << "no responsible folder found";
        return;
    }

    const QVariantMap initialProperties{
        {"accountState", QVariant::fromValue(folder->accountState())},
        {"localPath", localPath},
    };

    QQmlComponent fileDetailsDialog(trayEngine(), QStringLiteral("qrc:/qml/src/gui/filedetails/FileDetailsWindow.qml"));

    if (!fileDetailsDialog.isError()) {
        const auto createdDialog = fileDetailsDialog.createWithInitialProperties(initialProperties);
        const auto dialog = qobject_cast<QQuickWindow*>(createdDialog);

        if(!dialog) {
            qCWarning(lcSystray) << "File details dialog window resulted in creation of object that was not a window!";
            return;
        }

        _fileDetailDialogs.append(dialog);

        dialog->show();
        dialog->raise();
        dialog->requestActivate();

    } else {
        qCWarning(lcSystray) << fileDetailsDialog.errorString();
    }
}

void Systray::createShareDialog(const QString &localPath)
{
    createFileDetailsDialog(localPath);
    Q_EMIT showFileDetailsPage(localPath, FileDetailsPage::Sharing);
}

void Systray::createFileActivityDialog(const QString &localPath)
{
    createFileDetailsDialog(localPath);
    Q_EMIT showFileDetailsPage(localPath, FileDetailsPage::Activity);
}

void Systray::showFileActionsDialog(const QString &localPath)
{
    createFileActionsDialog(localPath);
}

#ifdef BUILD_FILE_PROVIDER_MODULE

void Systray::slotShowFileProviderFileActionsDialog(const QString &fileId, const QString &localPath, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier)
{
    createFileProviderFileActionsDialog(fileId, localPath, remoteItemPath, fileProviderDomainIdentifier);
}

void Systray::createFileProviderFileActionsDialog(const QString &fileId, const QString &localPath, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier)
{
    if (!_trayEngine) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath << "as no tray engine was available";
        return;
    }

    const auto accountState = AccountManager::instance()->accountFromFileProviderDomainIdentifier(fileProviderDomainIdentifier);
    if (!accountState) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath
                             << "no account found for domain identifier" << fileProviderDomainIdentifier;
        return;
    }

    createFileActionsDialogWithAccountState(localPath, accountState.data(), fileId, remoteItemPath);
}

#endif

void Systray::createFileActionsDialog(const QString &localPath)
{
    if (!_trayEngine) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath << "as no tray engine was available";
        return;
    }

    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath << "no responsible folder found";
        return;
    }

    createFileActionsDialogWithAccountState(localPath, folder->accountState());
}

void Systray::createFileActionsDialogWithAccountState(const QString &localPath, AccountState *accountState, const QString &fileId, const QString &remoteItemPath)
{
    if (!_trayEngine) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath << "as no tray engine was available";
        return;
    }

    if (!accountState) {
        qCWarning(lcSystray) << "Could not open file actions dialog for" << localPath << "no account state provided";
        return;
    }

    QQmlComponent fileActionsQml(trayEngine(), QStringLiteral("qrc:/qml/src/gui/integration/FileActionsWindow.qml"));

    if (fileActionsQml.isError()) {
        qCWarning(lcSystray) << fileActionsQml.errorString();
        qCWarning(lcSystray) << fileActionsQml.errors();
        return;
    }

    QFileInfo localFile{localPath};
    const auto shortLocalPath = localFile.fileName();

    const QVariantMap initialProperties{
        {"accountState", QVariant::fromValue(accountState)},
        {"fileId", fileId},
        {"remoteItemPath", remoteItemPath},
        {"shortLocalPath", shortLocalPath},
        {"localPath", localPath},
    };

    const auto fileActionsDialog = fileActionsQml.createWithInitialProperties(initialProperties);
    const auto dialog = qobject_cast<QQuickWindow*>(fileActionsDialog);

    if (!dialog) {
        qCWarning(lcSystray) << "File Actions dialog window resulted in creation of object that was not a window!";
        return;
    }

    dialog->show();
    dialog->raise();
    dialog->requestActivate();
}

void Systray::presentShareViewInTray(const QString &localPath)
{
    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        qCWarning(lcSystray) << "Could not open file details view in tray for" << localPath << "no responsible folder found";
        return;
    }
    qCDebug(lcSystray) << "Opening file details view in tray for " << localPath;

    Q_EMIT showFileDetails(folder->accountState(), localPath, FileDetailsPage::Sharing);
}

void Systray::presentFileActionsViewInSystray(const QString &localPath)
{
    qCDebug(lcSystray) << "Opening file actions view in tray for " << localPath;
    createFileActionsDialog(localPath);
}

void Systray::slotCurrentUserChanged()
{
    if (_trayEngine) {
        // Change ActivityModel
        _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
    }

    // Rebuild App list
    UserAppsModel::instance()->buildAppList();
}

void Systray::slotUpdateSyncPausedState()
{
    const auto folderMap = FolderMan::instance()->map();
    for (const auto folder : folderMap) {
        connect(folder, &Folder::syncPausedChanged, this, &Systray::slotUpdateSyncPausedState, Qt::UniqueConnection);
        if (!folder->syncPaused()) {
            _syncIsPaused = false;
            emit syncIsPausedChanged();
            return;
        }
    }

    _syncIsPaused = true;
    emit syncIsPausedChanged();
}

void Systray::slotUnpauseAllFolders()
{
    setPauseOnAllFoldersHelper(false);
}

void Systray::slotPauseAllFolders()
{
    setPauseOnAllFoldersHelper(true);
}

void Systray::slotSyncFoldersChanged(const OCC::Folder::Map &folderMap)
{
    if (const auto currentAnySyncFolders = !folderMap.isEmpty(); currentAnySyncFolders != _anySyncFolders) {
        _anySyncFolders = currentAnySyncFolders;
        emit anySyncFoldersChanged();
    }
}

void Systray::setPauseOnAllFoldersHelper(bool pause)
{
    // For some reason we get the raw pointer from Folder::accountState()
    // that's why we need a list of raw pointers for the call to contains
    // later on...
    const auto accounts = [=] {
        const auto ptrList = AccountManager::instance()->accounts();
        auto result = QList<AccountState *>();
        result.reserve(ptrList.size());
        std::transform(std::cbegin(ptrList), std::cend(ptrList), std::back_inserter(result), [](const AccountStatePtr &account) {
            return account.data();
        });
        return result;
    }();
    const auto folders = FolderMan::instance()->map();
    for (auto f : folders) {
        if (accounts.contains(f->accountState())) {
            f->setSyncPaused(pause);
            if (pause) {
                f->slotTerminateSync();
            }
        }
    }
}

QString Systray::windowTitle() const
{
    return Theme::instance()->appNameGUI();
}

bool Systray::useNormalWindow() const
{
    if (!isSystemTrayAvailable()) {
        return true;
    }

    ConfigFile cfg;
    return cfg.showMainDialogAsNormalWindow();
}

bool Systray::isOpen() const
{
    return _isOpen;
}

bool Systray::enableAddAccount() const
{
#if defined ENFORCE_SINGLE_ACCOUNT
    return AccountManager::instance()->accounts().isEmpty();
#else
    return true;
#endif
}

void Systray::setIsOpen(const bool isOpen)
{
    _isOpen = isOpen;
    Q_EMIT isOpenChanged();
}

void Systray::showMessage(const QString &title, const QString &message, MessageIcon icon)
{
#ifdef USE_FDO_NOTIFICATIONS
    if (QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        const QVariantMap hints = {{QStringLiteral("desktop-entry"), LINUX_APPLICATION_ID}};
        QList<QVariant> args = QList<QVariant>() << APPLICATION_NAME << quint32(0) << APPLICATION_ICON_NAME
                                                 << title << message << QStringList() << hints << qint32(-1);
        QDBusMessage method = QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE, "Notify");
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
        if (canOsXSendUserNotification()) {
        sendOsXUserNotification(title, message);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon);
    }
}

void Systray::showUpdateMessage(const QString &title, const QString &message, const QUrl &webUrl)
{
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    sendOsXUpdateNotification(title, message, webUrl);
#else // TODO: Implement custom notifications (i.e. actionable) for other OSes
    Q_UNUSED(webUrl);
    showMessage(title, message);
#endif
}

void Systray::showTalkMessage(const QString &title, const QString &message, const QString &token, const QString &replyTo, const AccountStatePtr &accountState)
{
#if defined(Q_OS_MACOS) && __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14 && defined(BUILD_OWNCLOUD_OSX_BUNDLE)
    sendOsXTalkNotification(title, message, token, replyTo, accountState);
#else // TODO: Implement custom notifications (i.e. actionable) for other OSes
    Q_UNUSED(replyTo)
    Q_UNUSED(token)
    Q_UNUSED(accountState)
    showMessage(title, message);
#endif
}

bool Systray::syncIsPaused() const
{
    return _syncIsPaused;
}

void Systray::setSyncIsPaused(const bool syncIsPaused)
{
    _syncIsPaused = syncIsPaused;
    if (_syncIsPaused) {
        slotPauseAllFolders();
    } else {
        slotUnpauseAllFolders();
    }
    emit syncIsPausedChanged();
}

bool Systray::anySyncFolders() const
{
    return _anySyncFolders;
}

/********************************************************************************************/
/* Helper functions for cross-platform tray icon position and taskbar orientation detection */
/********************************************************************************************/

void Systray::positionWindowAtTray(QQuickWindow *window) const
{
    if (useNormalWindow()) {
        return;
    }

    // need to store the current window size before moving the window to another screen,
    // otherwise it is being incorrectly resized by the OS or Qt when switching to a screen
    // with a different DPI setting
    const auto initialSize = window->size();
    const auto position = computeWindowPosition(initialSize.width(), initialSize.height());
    window->setPosition(position);
    window->setScreen(currentScreen());
    window->resize(initialSize);
}

void Systray::positionWindowAtScreenCenter(QQuickWindow *window) const
{
    if(!useNormalWindow()) {
        window->setScreen(currentScreen());
        const QPoint windowAdjustment(window->geometry().width() / 2, window->geometry().height() / 2);
        const auto position = currentScreen()->availableGeometry().center() - windowAdjustment;
        window->setPosition(position);
    }
}

void Systray::forceWindowInit(QQuickWindow *window) const
{
    // HACK: At least on Windows, if the systray window is not shown at least once
    // it can prevent session handling to carry on properly, so we show/hide it here
    // this shouldn't flicker
    window->show();
    window->hide();
    
#ifdef Q_OS_MACOS
    // On macOS we need to designate the tray window as visible on all spaces and
    // at the menu bar level, otherwise showing it can cause the current spaces to
    // change, or the window could be obscured by another window that shouldn't
    // normally cover a menu.
    OCC::setTrayWindowLevelAndVisibleOnAllSpaces(window);
#endif
}

void Systray::positionNotificationWindow(QQuickWindow *window) const
{
    if (!useNormalWindow()) {
        window->setScreen(currentScreen());
        if(geometry().isValid()) {
            // On OSes where the QSystemTrayIcon geometry method isn't borked, we can actually figure out where the system tray is located
            // We can therefore use our normal routines
            const auto position = computeNotificationPosition(window->width(), window->height());
            window->setPosition(position);
        } else if (QProcessEnvironment::systemEnvironment().contains(QStringLiteral("XDG_CURRENT_DESKTOP")) &&
                   (QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CURRENT_DESKTOP")).contains(QStringLiteral("GNOME")))) {
            // We can safely hardcode the top-right position for the notification when running GNOME
            const auto position = computeNotificationPosition(window->width(), window->height(), 0, NotificationPosition::TopRight);
            window->setPosition(position);
        } else {
            // For other DEs we play it safe and place the notification in the centre of the screen
            positionWindowAtScreenCenter(window);
        }
        // TODO: Get actual notification positions for the DEs
    }
}

QScreen *Systray::currentScreen() const
{
    const auto screen = QGuiApplication::screenAt(QCursor::pos());

    if(screen) {
        return screen;
    }
    // Didn't find anything matching the cursor position,
    // falling back to the primary screen
    return QGuiApplication::primaryScreen();
}

Systray::TaskBarPosition Systray::taskbarOrientation() const
{
// macOS: Always on top
#if defined(Q_OS_MACOS)
    return TaskBarPosition::Top;
// Windows: Check registry for actual taskbar orientation
#elif defined(Q_OS_WIN)
    auto taskbarPositionSubkey = QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3");
    if (!Utility::registryKeyExists(HKEY_CURRENT_USER, taskbarPositionSubkey)) {
        // Windows 7
        taskbarPositionSubkey = QStringLiteral("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects2");
    }
    if (!Utility::registryKeyExists(HKEY_CURRENT_USER, taskbarPositionSubkey)) {
        return TaskBarPosition::Bottom;
    }
    auto taskbarPosition = Utility::registryGetKeyValue(HKEY_CURRENT_USER, taskbarPositionSubkey, "Settings");
    switch (taskbarPosition.toInt()) {
    // Mapping windows binary value (0 = left, 1 = top, 2 = right, 3 = bottom) to qml logic (0 = bottom, 1 = left...)
    case 0:
        return TaskBarPosition::Left;
    case 1:
        return TaskBarPosition::Top;
    case 2:
        return TaskBarPosition::Right;
    case 3:
        return TaskBarPosition::Bottom;
    default:
        return TaskBarPosition::Bottom;
    }
// Probably Linux
#else
    const auto screenRect = currentScreenRect();
    const auto trayIconCenter = calcTrayIconCenter();

    const auto distBottom = screenRect.bottom() - trayIconCenter.y();
    const auto distRight = screenRect.right() - trayIconCenter.x();
    const auto distLeft = trayIconCenter.x() - screenRect.left();
    const auto distTop = trayIconCenter.y() - screenRect.top();

    const auto minDist = std::min({distRight, distTop, distBottom});

    if (minDist == distBottom) {
        return TaskBarPosition::Bottom;
    } else if (minDist == distLeft) {
        return TaskBarPosition::Left;
    } else if (minDist == distTop) {
        return TaskBarPosition::Top;
    } else {
        return TaskBarPosition::Right;
    }
#endif
}

QRect Systray::currentScreenRect() const
{
    const auto screen = currentScreen();
    Q_ASSERT(screen);
    return screen->geometry();
}

QRect Systray::currentAvailableScreenRect() const
{
    const auto screen = currentScreen();
    Q_ASSERT(screen);
    return screen->availableGeometry();
}

QPoint Systray::computeWindowReferencePoint() const
{
    constexpr auto spacing = 4;
    const auto trayIconCenter = calcTrayIconCenter();
    const auto taskbarScreenEdge = taskbarOrientation();
    const auto screenRect = currentAvailableScreenRect();

    qCDebug(lcSystray) << "screenRect:" << screenRect;
    qCDebug(lcSystray) << "taskbarScreenEdge:" << taskbarScreenEdge;
    qCDebug(lcSystray) << "trayIconCenter:" << trayIconCenter;

    switch(taskbarScreenEdge) {
    case TaskBarPosition::Bottom:
        return {
            trayIconCenter.x(),
            screenRect.bottom() - spacing
        };
    case TaskBarPosition::Left:
        return {
            screenRect.left() + spacing,
            trayIconCenter.y()
        };
    case TaskBarPosition::Top:
        return {
            trayIconCenter.x(),
            screenRect.top() + spacing
        };
    case TaskBarPosition::Right:
        return {
            screenRect.right() - spacing,
            trayIconCenter.y()
        };
    }
    Q_UNREACHABLE();
}

QPoint Systray::computeNotificationReferencePoint(int spacing, NotificationPosition position) const
{
    auto trayIconCenter = calcTrayIconCenter();
    auto taskbarScreenEdge = taskbarOrientation();
    const auto screenRect = currentAvailableScreenRect();
    
    if(position == NotificationPosition::TopLeft) {
        taskbarScreenEdge = TaskBarPosition::Top;
        trayIconCenter = QPoint(0, 0);
    } else if(position == NotificationPosition::TopRight) {
        taskbarScreenEdge = TaskBarPosition::Top;
        trayIconCenter = QPoint(screenRect.width(), 0);
    } else if(position == NotificationPosition::BottomLeft) {
        taskbarScreenEdge = TaskBarPosition::Bottom;
        trayIconCenter = QPoint(0, screenRect.height());
    } else if(position == NotificationPosition::BottomRight) {
        taskbarScreenEdge = TaskBarPosition::Bottom;
        trayIconCenter = QPoint(screenRect.width(), screenRect.height());
    }

    qCDebug(lcSystray) << "screenRect:" << screenRect;
    qCDebug(lcSystray) << "taskbarScreenEdge:" << taskbarScreenEdge;
    qCDebug(lcSystray) << "trayIconCenter:" << trayIconCenter;

    switch(taskbarScreenEdge) {
    case TaskBarPosition::Bottom:
        return {
            trayIconCenter.x() < screenRect.center().x() ? screenRect.left() + spacing :  screenRect.right() - spacing,
            screenRect.bottom() - spacing
        };
    case TaskBarPosition::Left:
        return {
            screenRect.left() + spacing,
            trayIconCenter.y() < screenRect.center().y() ? screenRect.top() + spacing : screenRect.bottom() - spacing
        };
    case TaskBarPosition::Top:
        return {
            trayIconCenter.x() < screenRect.center().x() ? screenRect.left() + spacing :  screenRect.right() - spacing,
            screenRect.top() + spacing
        };
    case TaskBarPosition::Right:
        return {
            screenRect.right() - spacing,
            trayIconCenter.y() < screenRect.center().y() ? screenRect.top() + spacing : screenRect.bottom() - spacing
        };
    }
    Q_UNREACHABLE();
}

QRect Systray::computeWindowRect(int spacing, const QPoint &topLeft, const QPoint &bottomRight) const
{
    const auto screenRect = currentScreenRect();
    const auto rect = QRect(topLeft, bottomRight);
    auto offset = QPoint();

    if (rect.left() < screenRect.left()) {
        offset.setX(screenRect.left() - rect.left() + spacing);
    } else if (rect.right() > screenRect.right()) {
        offset.setX(screenRect.right() - rect.right() - spacing);
    }

    if (rect.top() < screenRect.top()) {
        offset.setY(screenRect.top() - rect.top() + spacing);
    } else if (rect.bottom() > screenRect.bottom()) {
        offset.setY(screenRect.bottom() - rect.bottom() - spacing);
    }

    return rect.translated(offset);
}

QPoint Systray::computeWindowPosition(int width, int height) const
{
    constexpr auto spacing = 4;
    const auto referencePoint = computeWindowReferencePoint();

    const auto taskbarScreenEdge = taskbarOrientation();
    const auto screenRect = currentScreenRect();

    const auto topLeft = [=]() {
        switch(taskbarScreenEdge) {
        case TaskBarPosition::Bottom:
            return referencePoint - QPoint(width / 2, height);
        case TaskBarPosition::Left:
            return referencePoint;
        case TaskBarPosition::Top:
            return referencePoint - QPoint(width / 2, 0);
        case TaskBarPosition::Right:
            return referencePoint - QPoint(width, 0);
        }
        Q_UNREACHABLE();
    }();
    const auto bottomRight = topLeft + QPoint(width, height);
    const auto windowRect = computeWindowRect(spacing, topLeft, bottomRight);

    qCDebug(lcSystray) << "taskbarScreenEdge:" << taskbarScreenEdge;
    qCDebug(lcSystray) << "screenRect:" << screenRect;
    qCDebug(lcSystray) << "windowRect (reference)" << QRect(topLeft, bottomRight);
    qCDebug(lcSystray) << "windowRect (adjusted)" << windowRect;

    return windowRect.topLeft();
}

QPoint Systray::computeNotificationPosition(int width, int height, int spacing, NotificationPosition position) const
{
    const auto referencePoint = computeNotificationReferencePoint(spacing, position);

    auto trayIconCenter = calcTrayIconCenter();
    auto taskbarScreenEdge = taskbarOrientation();
    const auto screenRect = currentScreenRect();
        
    if(position == NotificationPosition::TopLeft) {
        taskbarScreenEdge = TaskBarPosition::Top;
        trayIconCenter = QPoint(0, 0);
    } else if(position == NotificationPosition::TopRight) {
        taskbarScreenEdge = TaskBarPosition::Top;
        trayIconCenter = QPoint(screenRect.width(), 0);
    } else if(position == NotificationPosition::BottomLeft) {
        taskbarScreenEdge = TaskBarPosition::Bottom;
        trayIconCenter = QPoint(0, screenRect.height());
    } else if(position == NotificationPosition::BottomRight) {
        taskbarScreenEdge = TaskBarPosition::Bottom;
        trayIconCenter = QPoint(screenRect.width(), screenRect.height());
    }
        
    const auto topLeft = [=]() {
        switch(taskbarScreenEdge) {
        case TaskBarPosition::Bottom:
            return trayIconCenter.x() < screenRect.center().x() ? referencePoint - QPoint(0, height) : referencePoint - QPoint(width, height);
        case TaskBarPosition::Left:
            return trayIconCenter.y() < screenRect.center().y() ? referencePoint : referencePoint - QPoint(0, height);
        case TaskBarPosition::Top:
            return trayIconCenter.x() < screenRect.center().x() ? referencePoint : referencePoint - QPoint(width, 0);
        case TaskBarPosition::Right:
            return trayIconCenter.y() < screenRect.center().y() ? referencePoint - QPoint(width, 0) : QPoint(width, height);
        }
        Q_UNREACHABLE();
    }();
    const auto bottomRight = topLeft + QPoint(width, height);
    const auto windowRect = computeWindowRect(spacing, topLeft, bottomRight);

    qCDebug(lcSystray) << "taskbarScreenEdge:" << taskbarScreenEdge;
    qCDebug(lcSystray) << "screenRect:" << screenRect;
    qCDebug(lcSystray) << "windowRect (reference)" << QRect(topLeft, bottomRight);
    qCDebug(lcSystray) << "windowRect (adjusted)" << windowRect;
    qCDebug(lcSystray) << "referencePoint" << referencePoint;

    return windowRect.topLeft();
}

QPoint Systray::calcTrayIconCenter() const
{
    const auto geo = geometry();
    if(geo.isValid()) {
        // QSystemTrayIcon::geometry() is broken for ages on most Linux DEs (invalid geometry returned)
        // thus we can use this only for Windows and macOS
        return geo.center();
    }

    // On Linux, fall back to mouse position (assuming tray icon is activated by mouse click)
    return QCursor::pos(currentScreen());
}

AccessManagerFactory::AccessManagerFactory()
    : QQmlNetworkAccessManagerFactory()
{
}

QNetworkAccessManager* AccessManagerFactory::create(QObject *parent)
{
    const auto am = new AccessManager(parent);
    const auto diskCache = new QNetworkDiskCache(am);
    diskCache->setCacheDirectory(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    am->setCache(diskCache);
    return am;
}

} // namespace OCC
