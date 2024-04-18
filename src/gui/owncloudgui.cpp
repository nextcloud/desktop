/*
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
#include "filedetails/datefieldbackend.h"
#include "filedetails/filedetails.h"
#include "filedetails/shareemodel.h"
#include "filedetails/sharemodel.h"
#include "filedetails/sortedsharemodel.h"
#include "tray/sortedactivitylistmodel.h"
#include "tray/syncstatussummary.h"
#include "tray/unifiedsearchresultslistmodel.h"

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

#ifdef BUILD_FILE_PROVIDER_MODULE
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
        this, &ownCloudGui::slotShutdown);

    ProgressDispatcher *pd = ProgressDispatcher::instance();
    connect(pd, &ProgressDispatcher::progressInfo, this,
        &ownCloudGui::slotUpdateProgress);

    FolderMan *folderMan = FolderMan::instance();
    connect(folderMan, &FolderMan::folderSyncStateChange,
        this, &ownCloudGui::slotSyncStateChange);

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

    qmlRegisterUncreatableType<QAbstractItemModel>("com.nextcloud.desktopclient", 1, 0, "QAbstractItemModel", "QAbstractItemModel");
    qmlRegisterUncreatableType<Activity>("com.nextcloud.desktopclient", 1, 0, "Activity", "Activity");
    qmlRegisterUncreatableType<TalkNotificationData>("com.nextcloud.desktopclient", 1, 0, "TalkNotificationData", "TalkNotificationData");
    qmlRegisterUncreatableType<UnifiedSearchResultsListModel>("com.nextcloud.desktopclient", 1, 0, "UnifiedSearchResultsListModel", "UnifiedSearchResultsListModel");
    qmlRegisterUncreatableType<UserStatus>("com.nextcloud.desktopclient", 1, 0, "UserStatus", "Access to Status enum");
    qmlRegisterUncreatableType<Sharee>("com.nextcloud.desktopclient", 1, 0, "Sharee", "Access to Type enum");

    qRegisterMetaTypeStreamOperators<Emoji>();

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
    slotComputeOverallSyncStatus();

    if (!folder) {
        return; // Valid, just a general GUI redraw was needed.
    }

    auto result = folder->syncResult();

    qCInfo(lcApplication) << "Sync state changed for folder " << folder->remoteUrl().toString() << ": " << result.statusString();

    if (result.status() == SyncResult::Success
        || result.status() == SyncResult::Problem
        || result.status() == SyncResult::SyncAbortRequested
        || result.status() == SyncResult::Error) {
        Logger::instance()->enterNextLogFile();
    }
}

void ownCloudGui::slotFoldersChanged()
{
    slotComputeOverallSyncStatus();
}

void ownCloudGui::slotOpenPath(const QString &path)
{
    showInFileManager(path);
}

void ownCloudGui::slotAccountStateChanged()
{
    slotComputeOverallSyncStatus();
}

void ownCloudGui::slotTrayMessageIfServerUnsupported(Account *account)
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

void ownCloudGui::slotComputeOverallSyncStatus()
{
    bool allSignedOut = true;
    bool allPaused = true;
    bool allDisconnected = true;
    QVector<AccountStatePtr> problemAccounts;
    auto setStatusText = [&](const QString &text) {
        // FIXME: So this doesn't do anything? Needs to be revisited
        Q_UNUSED(text)
        // Don't overwrite the status if we're currently syncing
        if (FolderMan::instance()->isAnySyncRunning())
            return;
        //_actionStatus->setText(text);
    };

    foreach (auto a, AccountManager::instance()->accounts()) {
        if (!a->isSignedOut()) {
            allSignedOut = false;
        }
        if (!a->isConnected()) {
            problemAccounts.append(a);
        } else {
            allDisconnected = false;
        }
    }
    foreach (Folder *f, FolderMan::instance()->map()) {
        if (!f->syncPaused()) {
            allPaused = false;
        }
    }

    if (!problemAccounts.empty()) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true));
        if (allDisconnected) {
            setStatusText(tr("Disconnected"));
        } else {
            setStatusText(tr("Disconnected from some accounts"));
        }
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        QStringList accountNames;
        foreach (AccountStatePtr a, problemAccounts) {
            accountNames.append(a->account()->displayName());
        }
        _tray->setToolTip(tr("Disconnected from %1").arg(accountNames.join(QLatin1String(", "))));
#else
        QStringList messages;
        messages.append(tr("Disconnected from accounts:"));
        foreach (AccountStatePtr a, problemAccounts) {
            QString message = tr("Account %1: %2").arg(a->account()->displayName(), a->stateString(a->state()));
            if (!a->connectionErrors().empty()) {
                message += QLatin1String("\n");
                message += a->connectionErrors().join(QLatin1String("\n"));
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
        setStatusText(tr("Signed out"));
        return;
    } else if (allPaused) {
        _tray->setIcon(Theme::instance()->syncStateIcon(SyncResult::Paused, true));
        _tray->setToolTip(tr("Account synchronization is disabled"));
        setStatusText(tr("Synchronization is paused"));
        return;
    }

    // display the info of the least successful sync (eg. do not just display the result of the latest sync)
    QString trayMessage;
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map map = folderMan->map();

    SyncResult::Status overallStatus = SyncResult::Undefined;
    bool hasUnresolvedConflicts = false;
    FolderMan::trayOverallStatus(map.values(), &overallStatus, &hasUnresolvedConflicts);

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
    if (map.count() > 0) {
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        trayMessage = folderMan->trayTooltipStatusString(overallStatus, hasUnresolvedConflicts, false);
#else
        QStringList allStatusStrings;
        foreach (Folder *folder, map.values()) {
            QString folderMessage = FolderMan::trayTooltipStatusString(
                folder->syncResult().status(),
                folder->syncResult().hasUnresolvedConflicts(),
                folder->syncPaused());
            allStatusStrings += tr("Folder %1: %2").arg(folder->shortGuiLocalPath(), folderMessage);
        }
        trayMessage = allStatusStrings.join(QLatin1String("\n"));
#endif
        _tray->setToolTip(trayMessage);

        if (overallStatus == SyncResult::Success || overallStatus == SyncResult::Problem) {
            if (hasUnresolvedConflicts) {
                setStatusText(tr("Unresolved conflicts"));
            } else {
                setStatusText(tr("Up to date"));
            }
        } else if (overallStatus == SyncResult::Paused) {
            setStatusText(tr("Synchronization is paused"));
        } else {
            setStatusText(tr("Error during synchronization"));
        }
    } else {
        _tray->setToolTip(tr("There are no sync folders configured."));
        setStatusText(tr("No sync folders configured"));
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

    // FIXME: Lots of messages computed for nothing in this method, needs revisiting
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

    if (progress.totalSize() == 0) {
        qint64 currentFile = progress.currentFile();
        qint64 totalFileCount = qMax(progress.totalFiles(), currentFile);
        QString msg;
        if (progress.trustEta()) {
            msg = tr("Syncing %1 of %2 (%3 left)")
                      .arg(currentFile)
                      .arg(totalFileCount)
                      .arg(Utility::durationToDescriptiveString2(progress.totalProgress().estimatedEta));
        } else {
            msg = tr("Syncing %1 of %2")
                      .arg(currentFile)
                      .arg(totalFileCount);
        }
        //_actionStatus->setText(msg);
    } else {
        QString totalSizeStr = Utility::octetsToString(progress.totalSize());
        QString msg;
        if (progress.trustEta()) {
            msg = tr("Syncing %1 (%2 left)")
                      .arg(totalSizeStr, Utility::durationToDescriptiveString2(progress.totalProgress().estimatedEta));
        } else {
            msg = tr("Syncing %1")
                      .arg(totalSizeStr);
        }
        //_actionStatus->setText(msg);
    }

    if (!progress._lastCompletedItem.isEmpty()) {

        QString kindStr = Progress::asResultString(progress._lastCompletedItem);
        QString timeStr = QTime::currentTime().toString("hh:mm");
        QString actionText = tr("%1 (%2, %3)").arg(progress._lastCompletedItem._file, kindStr, timeStr);
        auto *action = new QAction(actionText, this);
        Folder *f = FolderMan::instance()->folder(folder);
        if (f) {
            QString fullPath = f->path() + '/' + progress._lastCompletedItem._file;
            if (QFile(fullPath).exists()) {
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
        auto list = AccountManager::instance()->accounts();
        foreach (const auto &a, list) {
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

    foreach (const auto &ai, list) {
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
    _app->quit();
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

} // end namespace
