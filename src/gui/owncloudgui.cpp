/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "owncloudgui.h"

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "application.h"
#include "callstatechecker.h"
#include "emojimodel.h"
#include "fileactivitylistmodel.h"
#include "folderman.h"
#include "guiutility.h"
#include "logbrowser.h"
#include "logger.h"
#include "openfilemanager.h"
#include "owncloudsetupwizard.h"
#include "progressdispatcher.h"
#include "settingsdialog.h"
#include "theme.h"
#include "wheelhandler.h"
#include "syncconflictsmodel.h"
#include "syncengine.h"
#include "filedetails/datefieldbackend.h"
#include "filedetails/filedetails.h"
#include "filedetails/shareemodel.h"
#include "filedetails/sharemodel.h"
#include "filedetails/sortedsharemodel.h"
#include "tray/sortedactivitylistmodel.h"
#include "tray/syncstatussummary.h"
#include "tray/unifiedsearchresultslistmodel.h"
#include "integration/fileactionsmodel.h"
#include "filesystem.h"

#ifdef WITH_LIBCLOUDPROVIDERS
#include "cloudproviders/cloudprovidermanager.h"
#endif

#include <QQmlApplicationEngine>
#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QSignalMapper>
#ifdef WITH_LIBCLOUDPROVIDERS
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusInterface>
#endif

#include <QAbstractItemModel>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQmlContext>

#ifdef Q_OS_MACOS
#include "foregroundbackground_interface.h"
#endif

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovider.h"
#include "macOS/fileproviderdomainmanager.h"
#include "macOS/fileproviderservice.h"
#include "macOS/fileprovidersettingscontroller.h"
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcOwnCloudGui, "com.nextcloud.owncloudgui")

const char propertyAccountC[] = "oc_account";

ownCloudGui::ownCloudGui(Application *parent)
    : QObject(parent)
    , _tray(nullptr)
    , _settingsDialog(nullptr)
    , _logBrowser(nullptr)
#ifdef WITH_LIBCLOUDPROVIDERS
    , _bus(QDBusConnection::sessionBus())
#endif
    , _app(parent)
{
    _tray = Systray::instance();
    _tray->setTrayEngine(new QQmlApplicationEngine(this));
    // for the beginning, set the offline icon until the account was verified
    _tray->setIcon(Theme::instance()->folderOfflineIcon(/*systray?*/ true));

    _tray->show();

    connect(_tray.data(), &QSystemTrayIcon::activated,
        this, &ownCloudGui::slotTrayClicked);

    connect(_tray.data(), &Systray::openHelp,
        this, &ownCloudGui::slotHelp);

    connect(_tray.data(), &Systray::openAccountWizard,
        this, &ownCloudGui::slotNewAccountWizard);

    connect(_tray.data(), &Systray::openSettings,
        this, &ownCloudGui::slotShowSettings);

    connect(_tray.data(), &Systray::shutdown,
        this, &QCoreApplication::quit);

    ProgressDispatcher *pd = ProgressDispatcher::instance();
    connect(pd, &ProgressDispatcher::progressInfo, this,
        &ownCloudGui::slotUpdateProgress);

    FolderMan *folderMan = FolderMan::instance();
    connect(folderMan, &FolderMan::folderSyncStateChange, this, &ownCloudGui::slotSyncStateChange);
    connect(folderMan, &FolderMan::folderSyncStateChange, this, &ownCloudGui::slotComputeOverallSyncStatus);


#ifdef BUILD_FILE_PROVIDER_MODULE
    connect(Mac::FileProvider::instance()->service(), &Mac::FileProviderService::syncStateChanged, this, &ownCloudGui::slotComputeOverallSyncStatus);
    connect(Mac::FileProvider::instance()->service(), &Mac::FileProviderService::showFileActionsDialog, _tray.data(), &Systray::slotShowFileProviderFileActionsDialog);
#endif

    connect(Logger::instance(), &Logger::guiLog, this, &ownCloudGui::slotShowTrayMessage);
    connect(Logger::instance(), &Logger::guiMessage, this, &ownCloudGui::slotShowGuiMessage);

    qmlRegisterType<SyncStatusSummary>("com.nextcloud.desktopclient", 1, 0, "SyncStatusSummary");
    qmlRegisterType<EmojiModel>("com.nextcloud.desktopclient", 1, 0, "EmojiModel");
    qmlRegisterType<UserStatusSelectorModel>("com.nextcloud.desktopclient", 1, 0, "UserStatusSelectorModel");
    qmlRegisterType<ActivityListModel>("com.nextcloud.desktopclient", 1, 0, "ActivityListModel");
    qmlRegisterType<FileActivityListModel>("com.nextcloud.desktopclient", 1, 0, "FileActivityListModel");
    qmlRegisterType<SortedActivityListModel>("com.nextcloud.desktopclient", 1, 0, "SortedActivityListModel");
    qmlRegisterType<WheelHandler>("com.nextcloud.desktopclient", 1, 0, "WheelHandler");
    qmlRegisterType<CallStateChecker>("com.nextcloud.desktopclient", 1, 0, "CallStateChecker");
    qmlRegisterType<Quick::DateFieldBackend>("com.nextcloud.desktopclient", 1, 0, "DateFieldBackend");
    qmlRegisterType<FileDetails>("com.nextcloud.desktopclient", 1, 0, "FileDetails");
    qmlRegisterType<ShareModel>("com.nextcloud.desktopclient", 1, 0, "ShareModel");
    qmlRegisterType<ShareeModel>("com.nextcloud.desktopclient", 1, 0, "ShareeModel");
    qmlRegisterType<SortedShareModel>("com.nextcloud.desktopclient", 1, 0, "SortedShareModel");
    qmlRegisterType<SyncConflictsModel>("com.nextcloud.desktopclient", 1, 0, "SyncConflictsModel");
    qmlRegisterType<FileActionsModel>("com.nextcloud.desktopclient", 1, 0, "FileActionsModel");

    qmlRegisterUncreatableType<QAbstractItemModel>("com.nextcloud.desktopclient", 1, 0, "QAbstractItemModel", "QAbstractItemModel");
    qmlRegisterUncreatableType<Activity>("com.nextcloud.desktopclient", 1, 0, "activity", "Activity");
    qmlRegisterUncreatableType<TalkNotificationData>("com.nextcloud.desktopclient", 1, 0, "talkNotificationData", "TalkNotificationData");
    qmlRegisterUncreatableType<UnifiedSearchResultsListModel>("com.nextcloud.desktopclient", 1, 0, "UnifiedSearchResultsListModel", "UnifiedSearchResultsListModel");
    qmlRegisterUncreatableType<UserStatus>("com.nextcloud.desktopclient", 1, 0, "userStatus", "Access to Status enum");
    qmlRegisterUncreatableType<Sharee>("com.nextcloud.desktopclient", 1, 0, "sharee", "Access to Type enum");
    qmlRegisterUncreatableType<ClientSideEncryptionTokenSelector>("com.nextcloud.desktopclient", 1, 0, "ClientSideEncryptionTokenSelector", "Access to the certificate selector");

    qRegisterMetaType<ActivityListModel *>("ActivityListModel*");
    qRegisterMetaType<UnifiedSearchResultsListModel *>("UnifiedSearchResultsListModel*");
    qRegisterMetaType<UserStatus>("UserStatus");
    qRegisterMetaType<SharePtr>("SharePtr");
    qRegisterMetaType<ShareePtr>("ShareePtr");
    qRegisterMetaType<Sharee>("Sharee");
    qRegisterMetaType<OCC::ActivityList>("ActivityList");

    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserModel", UserModel::instance());
    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserAppsModel", UserAppsModel::instance());
    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Theme", Theme::instance());
    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Systray", Systray::instance());

#ifdef BUILD_FILE_PROVIDER_MODULE
    qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "FileProviderSettingsController", Mac::FileProviderSettingsController::instance());
#endif
}

void ownCloudGui::createTray()
{
    _tray->create();
}

#ifdef WITH_LIBCLOUDPROVIDERS
void ownCloudGui::setupCloudProviders()
{
    new CloudProviderManager(this);
}

bool ownCloudGui::cloudProviderApiAvailable()
{
    if (!_bus.isConnected()) {
        return false;
    }
    QDBusInterface dbus_iface("org.freedesktop.CloudProviderManager", "/org/freedesktop/CloudProviderManager",
                              "org.freedesktop.CloudProvider.Manager1", _bus);

    if (!dbus_iface.isValid()) {
        qCInfo(lcApplication) << "DBus interface unavailable";
        return false;
    }
    return true;
}
#endif

// This should rather be in application.... or rather in ConfigFile?
void ownCloudGui::slotOpenSettingsDialog()
{
    // if account is set up, start the configuration wizard.
    if (!AccountManager::instance()->accounts().isEmpty()) {
        if (_settingsDialog.isNull() || QApplication::activeWindow() != _settingsDialog) {
            slotShowSettings();
        } else {
            _settingsDialog->close();
        }
    } else {
        qCInfo(lcApplication) << "No configured folders yet, starting setup wizard";
        slotNewAccountWizard();
    }
}

void ownCloudGui::slotOpenMainDialog()
{
    _tray->showWindow();
}

void ownCloudGui::slotTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick && UserModel::instance()->currentUser()->hasLocalFolder()) {
        UserModel::instance()->openCurrentAccountLocalFolder();
    } else if (reason == QSystemTrayIcon::Trigger) {
        if (OwncloudSetupWizard::bringWizardToFrontIfVisible()) {
            // brought wizard to front
        } else if (_tray->raiseDialogs()) {
            // Brings dialogs hidden by other apps to front, returns true if any raised
        } else if (_tray->isOpen()) {
            _tray->hideWindow();
        } else {
            if (AccountManager::instance()->accounts().isEmpty()) {
                this->slotOpenSettingsDialog();
            } else {
                _tray->showWindow();
            }

        }
    }
    // FIXME: Also make sure that any auto updater dialogue https://github.com/owncloud/client/issues/5613
    // or SSL error dialog also comes to front.
}

void ownCloudGui::slotSyncStateChange(Folder *folder)
{
    if (!folder) {
        return; // Valid, just a general GUI redraw was needed.
    }

    auto result = folder->syncResult();

    qCInfo(lcApplication) << "Sync state changed for folder " << folder->remoteUrl().toString() << ": " << result.statusString();

    if (result.status() == SyncResult::Success
        || result.status() == SyncResult::Problem
        || result.status() == SyncResult::SyncAbortRequested
        || result.status() == SyncResult::Error) {
        Logger::instance()->enterNextLogFile(QStringLiteral("nextcloud.log"), OCC::Logger::LogType::Log);
    }
}

void ownCloudGui::slotOpenPath(const QString &path)
{
    showInFileManager(path);
}

void ownCloudGui::slotTrayMessageIfServerUnsupported(const AccountPtr &account)
{
    if (account->serverVersionUnsupported()) {
        slotShowTrayMessage(
            tr("Unsupported Server Version"),
            tr("The server on account %1 runs an unsupported version %2. "
               "Using this client with unsupported server versions is untested and "
               "potentially dangerous. Proceed at your own risk.")
                .arg(account->displayName(), account->serverVersion()));
    }
}

void ownCloudGui::slotNeedToAcceptTermsOfService(const OCC::AccountPtr &account,
                                                 const OCC::AccountState::State state)
{
    if (state == AccountState::NeedToSignTermsOfService) {
        slotShowTrayMessage(
            tr("Terms of service"),
            tr("Your account %1 requires you to accept the terms of service of your server. "
               "You will be redirected to %2 to acknowledge that you have read it and agrees with it.")
                .arg(account->displayName(), account->url().toString()));
        QDesktopServices::openUrl(account->url());
    }
}

void ownCloudGui::slotComputeOverallSyncStatus()
{
    bool allSignedOut = true;
    bool allPaused = true;
    QVector<AccountStatePtr> problemAccounts;

    const auto &allAccounts = AccountManager::instance()->accounts();

    for (const auto &account : allAccounts) {
        if (!account->isSignedOut()) {
            allSignedOut = false;
        }

        if (!account->isConnected()) {
            problemAccounts.append(account);
        }
    }
    for (const auto folder : FolderMan::instance()->map()) {
        if (!folder->syncPaused()) {
            allPaused = false;
        }
    }

#ifdef BUILD_FILE_PROVIDER_MODULE
    QList<QString> problemFileProviderAccounts;
    QList<QString> syncingFileProviderAccounts;
    QList<QString> successFileProviderAccounts;
    QList<QString> idleFileProviderAccounts;

    for (const auto &accountState : allAccounts) {
        const auto account = accountState->account();
        const auto userIdAtHostWithPort = account->userIdAtHostWithPort();

        if (!Mac::FileProviderSettingsController::instance()->vfsEnabledForAccount(userIdAtHostWithPort)) {
            continue;
        }

        allPaused = false;
        const auto fileProvider = Mac::FileProvider::instance();
        const auto accountFpId = account->fileProviderDomainIdentifier();
        const auto displayName = account->displayName();
        const auto accountTooltipLabel = displayName.isEmpty() ? userIdAtHostWithPort : displayName;

        if (!fileProvider->xpc()->fileProviderDomainReachable(accountFpId)) {
            problemFileProviderAccounts.append(accountTooltipLabel);
        } else {
            switch (fileProvider->service()->latestReceivedSyncStatusForAccount(accountState->account())) {
            case SyncResult::Undefined:
            case SyncResult::NotYetStarted:
                idleFileProviderAccounts.append(accountTooltipLabel);
                break;
            case SyncResult::SyncPrepare:
            case SyncResult::SyncRunning:
            case SyncResult::SyncAbortRequested:
                syncingFileProviderAccounts.append(accountTooltipLabel);
                break;
            case SyncResult::Success:
                successFileProviderAccounts.append(accountTooltipLabel);
                break;
            case SyncResult::Problem:
            case SyncResult::Error:
            case SyncResult::SetupError:
                problemFileProviderAccounts.append(accountTooltipLabel);
                break;
            case SyncResult::Paused: // This is not technically possible with VFS
                break;
            }
        }
    }
#endif

    if (!problemAccounts.empty()) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true));
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        QStringList accountNames;
        for (const AccountStatePtr &a : problemAccounts) {
            accountNames.append(a->account()->displayName());
        }
        _tray->setToolTip(tr("Disconnected from %1").arg(accountNames.join(QLatin1String(", "))));
#else
        QStringList messages;
        messages.append(tr("Disconnected from accounts:"));
        for (const auto &accountState : std::as_const(problemAccounts)) {
            QString message = tr("Account %1: %2").arg(accountState->account()->displayName(), accountState->stateString(accountState->state()));
            if (!accountState->connectionErrors().empty()) {
                message += QLatin1String("\n");
                message += accountState->connectionErrors().join(QLatin1String("\n"));
            }
            messages.append(message);
        }
        _tray->setToolTip(messages.join(QLatin1String("\n\n")));
#endif
        return;
    }

    if (allSignedOut) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true));
        _tray->setToolTip(tr("Please sign in"));
        return;
    } else if (allPaused) {
        _tray->setIcon(Theme::instance()->syncStateIcon(SyncResult::Paused, true));
        _tray->setToolTip(tr("Account synchronization is disabled"));
        return;
    }

    // display the info of the least successful sync (eg. do not just display the result of the latest sync)
    QString trayMessage;
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map map = folderMan->map();

    SyncResult::Status overallStatus = SyncResult::Undefined;
    bool hasUnresolvedConflicts = false;
    ProgressInfo *overallProgressInfo = nullptr;
    FolderMan::trayOverallStatus(map.values(), &overallStatus, &hasUnresolvedConflicts, &overallProgressInfo);

#ifdef BUILD_FILE_PROVIDER_MODULE
    if (!problemFileProviderAccounts.isEmpty()) {
        overallStatus = SyncResult::Problem;
    } else if (!syncingFileProviderAccounts.isEmpty() &&
               overallStatus != SyncResult::SyncRunning &&
               overallStatus != SyncResult::Problem &&
               overallStatus != SyncResult::Error &&
               overallStatus != SyncResult::SetupError) {
        overallStatus = SyncResult::SyncRunning;
    } else if ((!successFileProviderAccounts.isEmpty() || (problemFileProviderAccounts.isEmpty() && syncingFileProviderAccounts.isEmpty() && !idleFileProviderAccounts.isEmpty())) &&
               overallStatus != SyncResult::SyncRunning &&
               overallStatus != SyncResult::Problem &&
               overallStatus != SyncResult::Error &&
               overallStatus != SyncResult::SetupError) {
        overallStatus = SyncResult::Success;
    }
#endif

    // If the sync succeeded but there are unresolved conflicts,
    // show the problem icon!
    auto iconStatus = overallStatus;
    if (iconStatus == SyncResult::Success && hasUnresolvedConflicts) {
        iconStatus = SyncResult::Problem;
    }

    // If we don't get a status for whatever reason, that's a Problem
    if (iconStatus == SyncResult::Undefined) {
        iconStatus = SyncResult::Problem;
    }

    QIcon statusIcon = Theme::instance()->syncStateIcon(iconStatus, true);
    _tray->setIcon(statusIcon);

    // create the tray blob message, check if we have an defined state
#ifdef BUILD_FILE_PROVIDER_MODULE
    if (!map.isEmpty() || !syncingFileProviderAccounts.isEmpty() || !successFileProviderAccounts.isEmpty() || !problemFileProviderAccounts.isEmpty()) {
#else
    if (map.count() > 0) {
#endif
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        trayMessage = folderMan->trayTooltipStatusString(overallStatus, hasUnresolvedConflicts, false, overallProgressInfo);
#else
        QStringList allStatusStrings;
        const auto folders = map.values();
        for (const auto folder : folders) {
            QString folderMessage = FolderMan::trayTooltipStatusString(folder->syncResult().status(),
                                                                       folder->syncResult().hasUnresolvedConflicts(),
                                                                       folder->syncPaused(),
                                                                       folder->syncEngine().progressInfo());
            //: Example text: "Nextcloud: Syncing 25MB (3 minutes left)"   (%1 is the folder name to be synced, %2 a status message for that folder)
            allStatusStrings += tr("%1: %2").arg(folder->shortGuiLocalPath(), folderMessage);
        }
#ifdef BUILD_FILE_PROVIDER_MODULE
        for (const auto &accountId : syncingFileProviderAccounts) {
            allStatusStrings += tr("macOS VFS for %1: Sync is running.").arg(accountId);
        }
        for (const auto &accountId : successFileProviderAccounts) {
            allStatusStrings += tr("macOS VFS for %1: Last sync was successful.").arg(accountId);
        }
        for (const auto &accountId : problemFileProviderAccounts) {
            allStatusStrings += tr("macOS VFS for %1: A problem was encountered.").arg(accountId);
        }
#endif
        trayMessage = allStatusStrings.join(QLatin1String("\n"));
#endif
        _tray->setToolTip(trayMessage);
    } else {
        _tray->setToolTip(tr("There are no sync folders configured."));
    }
}

void ownCloudGui::hideAndShowTray()
{
    _tray->hide();
    _tray->show();
}

void ownCloudGui::slotShowTrayMessage(const QString &title, const QString &msg)
{
    qCDebug(lcOwnCloudGui) << "Going to show notification with title: '" << title << "' and message: '" << msg << "'";
    if (_tray) {
        _tray->showMessage(title, msg);
    } else {
        qCWarning(lcApplication) << "Tray not ready: " << msg;
    }
}

void ownCloudGui::slotShowTrayUpdateMessage(const QString &title, const QString &msg, const QUrl &webUrl)
{
    if(_tray) {
        _tray->showUpdateMessage(title, msg, webUrl);
    } else {
        qCWarning(lcApplication) << "Tray not ready: " << msg;
    }
}

/*
 * open the folder with the given Alias
 */
void ownCloudGui::slotFolderOpenAction(const QString &alias)
{
    Folder *f = FolderMan::instance()->folder(alias);
    if (f) {
        qCInfo(lcApplication) << "opening local url " << f->path();
        QUrl url = QUrl::fromLocalFile(f->path());

#ifdef Q_OS_WIN
        // work around a bug in QDesktopServices on Win32, see i-net
        QString filePath = f->path();

        if (filePath.startsWith(QLatin1String("\\\\")) || filePath.startsWith(QLatin1String("//")))
            url = QUrl::fromLocalFile(QDir::toNativeSeparators(filePath));
        else
            url = QUrl::fromLocalFile(filePath);
#endif
        QDesktopServices::openUrl(url);
    }
}

void ownCloudGui::slotUpdateProgress(const QString &folder, const ProgressInfo &progress)
{
    Q_UNUSED(folder);

    if (progress.status() == ProgressInfo::Discovery) {
#if 0
        if (!progress._currentDiscoveredRemoteFolder.isEmpty()) {
            _actionStatus->setText(tr("Checking for changes in remote \"%1\"")
                                       .arg(progress._currentDiscoveredRemoteFolder));
        } else if (!progress._currentDiscoveredLocalFolder.isEmpty()) {
            _actionStatus->setText(tr("Checking for changes in local \"%1\"")
                                       .arg(progress._currentDiscoveredLocalFolder));
        }
#endif
    } else if (progress.status() == ProgressInfo::Done) {
        QTimer::singleShot(2000, this, &ownCloudGui::slotComputeOverallSyncStatus);
    }
    if (progress.status() != ProgressInfo::Propagation) {
        return;
    }

    slotComputeOverallSyncStatus();

    if (!progress._lastCompletedItem.isEmpty()) {

        QString kindStr = Progress::asResultString(progress._lastCompletedItem);
        QString timeStr = QTime::currentTime().toString("hh:mm");
        QString actionText = tr("%1 (%2, %3)").arg(progress._lastCompletedItem._file, kindStr, timeStr);
        auto *action = new QAction(actionText, this);
        Folder *f = FolderMan::instance()->folder(folder);
        if (f) {
            QString fullPath = f->path() + '/' + progress._lastCompletedItem._file;
            if (FileSystem::fileExists(fullPath)) {
                connect(action, &QAction::triggered, this, [this, fullPath] { this->slotOpenPath(fullPath); });
            } else {
                action->setEnabled(false);
            }
        }
        if (_recentItemsActions.length() > 5) {
            _recentItemsActions.takeFirst()->deleteLater();
        }
        _recentItemsActions.append(action);
    }
}

void ownCloudGui::slotLogin()
{
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        account->account()->resetRejectedCertificates();
        account->signIn();
    } else {
        const auto list = AccountManager::instance()->accounts();
        for (const auto &a : list) {
            a->signIn();
        }
    }
}

void ownCloudGui::slotLogout()
{
    auto list = AccountManager::instance()->accounts();
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        list.clear();
        list.append(account);
    }

    for (const auto &ai : std::as_const(list)) {
        ai->signOutByUi();
    }
}

void ownCloudGui::slotNewAccountWizard()
{
#if defined ENFORCE_SINGLE_ACCOUNT
    if (!AccountManager::instance()->accounts().isEmpty()) {
        return;
    }
#endif
    OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)));
}

void ownCloudGui::slotShowGuiMessage(const QString &title, const QString &message)
{
    auto *msgBox = new QMessageBox;
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setText(message);
    msgBox->setWindowTitle(title);
    msgBox->setIcon(QMessageBox::Information);
    msgBox->open();
}

void ownCloudGui::slotShowSettings()
{
    if (_settingsDialog.isNull()) {
        _settingsDialog = new SettingsDialog(this);
        _settingsDialog->setAttribute(Qt::WA_DeleteOnClose, true);

#ifdef Q_OS_MACOS
        auto *fgbg = new ForegroundBackground();
        _settingsDialog->installEventFilter(fgbg);
#endif

        connect(_tray.data(), &Systray::hideSettingsDialog,
                _settingsDialog.data(), &SettingsDialog::close);

        _settingsDialog->show();
    }
    raiseDialog(_settingsDialog.data());
}

void ownCloudGui::slotSettingsDialogActivated()
{
    emit isShowingSettingsDialog();
}

void ownCloudGui::slotShowSyncProtocol()
{
    slotShowSettings();
    //_settingsDialog->showActivityPage();
}


void ownCloudGui::slotShutdown()
{
    // explicitly close windows. This is somewhat of a hack to ensure
    // that saving the geometries happens ASAP during a OS shutdown
    // those do delete on close
    if (!_settingsDialog.isNull())
        _settingsDialog->close();
    if (!_logBrowser.isNull())
        _logBrowser->deleteLater();
}

void ownCloudGui::slotToggleLogBrowser()
{
    if (_logBrowser.isNull()) {
        // init the log browser.
        _logBrowser = new LogBrowser;
        // ## TODO: allow new log name maybe?
    }

    if (_logBrowser->isVisible()) {
        _logBrowser->hide();
    } else {
        raiseDialog(_logBrowser);
    }
}

void ownCloudGui::slotOpenOwnCloud()
{
    if (auto account = qvariant_cast<AccountPtr>(sender()->property(propertyAccountC))) {
        Utility::openBrowser(account->url());
    }
}

void ownCloudGui::slotHelp()
{
    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void ownCloudGui::raiseDialog(QWidget *raiseWidget)
{
    if (raiseWidget && !raiseWidget->parentWidget()) {
        // Qt has a bug which causes parent-less dialogs to pop-under.
        raiseWidget->showNormal();
        raiseWidget->raise();
        raiseWidget->activateWindow();
#ifdef Q_OS_WIN
        // Windows disallows raising a Window when you're not the active application.
        // Use a common hack to attach to the active application
        const auto activeProcessId = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        if (activeProcessId != qApp->applicationPid()) {
            const auto threadId = GetCurrentThreadId();
            // don't step here with a debugger...
            if (AttachThreadInput(threadId, activeProcessId, true))
            {
                const auto hwnd = reinterpret_cast<HWND>(raiseWidget->winId());
                SetForegroundWindow(hwnd);
                SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                AttachThreadInput(threadId, activeProcessId, false);
            }
        }
#endif
    }
}


void ownCloudGui::slotShowShareDialog(const QString &localPath) const
{
    _tray->createShareDialog(localPath);
}

void ownCloudGui::slotShowFileActivityDialog(const QString &localPath) const
{
    _tray->createFileActivityDialog(localPath);
}

void ownCloudGui::slotShowFileActionsDialog(const QString &localPath) const
{
    _tray->showFileActionsDialog(localPath);
}

} // end namespace
