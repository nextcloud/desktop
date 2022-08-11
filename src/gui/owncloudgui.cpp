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
#include "aboutdialog.h"
#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "application.h"
#include "common/syncjournalfilerecord.h"
#include "configfile.h"
#include "creds/abstractcredentials.h"
#include "folderman.h"
#include "folderwizard/folderwizard.h"
#include "graphapi/drives.h"
#include "gui/accountsettings.h"
#include "guiutility.h"
#include "logbrowser.h"
#include "logger.h"
#include "openfilemanager.h"
#include "progressdispatcher.h"
#include "settingsdialog.h"
#include "setupwizardcontroller.h"
#include "sharedialog.h"
#include "theme.h"

#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QScreen>

#ifdef WITH_LIBCLOUDPROVIDERS
#include "libcloudproviders/libcloudproviders.h"
#include "selectivesyncdialog.h"
#endif

using namespace std::chrono_literals;

namespace {

using namespace OCC;

void setUpInitialSyncFolder(AccountStatePtr accountStatePtr, bool useVfs)
{
    auto folderMan = FolderMan::instance();

    // saves a bit of duplicate code
    auto addFolder = [folderMan, accountStatePtr, useVfs](const QString &localFolder, const QString &remotePath, const QUrl &webDavUrl, const QString &displayName = {}) {
        folderMan->addFolderFromWizard(accountStatePtr, localFolder, remotePath, webDavUrl, displayName, useVfs);
    };

    auto finalize = [accountStatePtr] {
        accountStatePtr->checkConnectivity();
        FolderMan::instance()->setSyncEnabled(true);
        FolderMan::instance()->scheduleAllFolders();
    };

    if (accountStatePtr->supportsSpaces()) {
        auto *drive = new GraphApi::Drives(accountStatePtr->account());

        QObject::connect(drive, &GraphApi::Drives::finishedSignal, [accountStatePtr, drive, addFolder, finalize] {
            if (drive->parseError().error == QJsonParseError::NoError) {
                const auto &drives = drive->drives();
                if (!drives.isEmpty()) {
                    const QDir localDir(accountStatePtr->account()->defaultSyncRoot());
                    FileSystem::setFolderMinimumPermissions(localDir.path());
                    Utility::setupFavLink(localDir.path());
                    for (const auto &d : drives) {
                        const QString name = GraphApi::Drives::getDriveDisplayName(d);
                        const QString folderName = FolderMan::instance()->findGoodPathForNewSyncFolder(localDir.filePath(name));
                        addFolder(folderName, {}, QUrl::fromEncoded(d.getRoot().getWebDavUrl().toUtf8()), name);
                    }
                    finalize();
                }
            }
        });

        drive->start();

        return;
    } else {
        addFolder(accountStatePtr->account()->defaultSyncRoot(), Theme::instance()->defaultServerFolder(), accountStatePtr->account()->davUrl());
        finalize();
    }
}
}

namespace OCC {

const char propertyAccountC[] = "oc_account";

ownCloudGui::ownCloudGui(Application *parent)
    : QObject(parent)
    , _tray(new Systray(this))
    , _settingsDialog(new SettingsDialog(this))
    , _recentActionsMenu(nullptr)
    , _app(parent)
{
    // for the beginning, set the offline icon until the account was verified
    _tray->setIcon(Theme::instance()->folderOfflineIcon(/*systray?*/ true, /*currently visible?*/ false));

    connect(_tray, &QSystemTrayIcon::activated,
        this, &ownCloudGui::slotTrayClicked);

    setupActions();
    setupContextMenu();

    _tray->show();

#ifdef WITH_LIBCLOUDPROVIDERS
    auto exporter = new LibCloudProviders(this);
    exporter->start();
    connect(exporter, &LibCloudProviders::showSettings, this, &ownCloudGui::slotShowSettings);
#endif

    ProgressDispatcher *pd = ProgressDispatcher::instance();
    connect(pd, &ProgressDispatcher::progressInfo, this,
        &ownCloudGui::slotUpdateProgress);

    FolderMan *folderMan = FolderMan::instance();
    connect(folderMan, &FolderMan::folderSyncStateChange,
        this, &ownCloudGui::slotSyncStateChange);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &ownCloudGui::updateContextMenuNeeded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved,
        this, &ownCloudGui::updateContextMenuNeeded);
}

ownCloudGui::~ownCloudGui()
{
    _settingsDialog->deleteLater();
}

// This should rather be in application.... or rather in ConfigFile?
void ownCloudGui::slotOpenSettingsDialog()
{
    // if account is set up, start the configuration wizard.
    if (!AccountManager::instance()->accounts().isEmpty()) {
        if (QApplication::activeWindow() != _settingsDialog) {
            slotShowSettings();
        } else {
            _settingsDialog->close();
        }
    } else {
        qCInfo(lcApplication) << "No configured folders yet, starting setup wizard";
        runNewAccountWizard();
    }
}

void ownCloudGui::slotTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
    if (_workaroundFakeDoubleClick) {
        static QElapsedTimer last_click;
        if (last_click.isValid() && last_click.elapsed() < 200) {
            return;
        }
        last_click.start();
    }

    // Left click
    if (reason == QSystemTrayIcon::Trigger) {
#ifdef Q_OS_MAC
        // on macOS, a left click always opens menu.
        // However if the settings dialog is already visible but hidden
        // by other applications, this will bring it to the front.
        if (_settingsDialog->isVisible()) {
            raiseDialog(_settingsDialog);
        }
#else
        slotOpenSettingsDialog();
#endif
    }
    // FIXME: Also make sure that any auto updater dialogue https://github.com/owncloud/client/issues/5613
    // or SSL error dialog also comes to front.
}

void ownCloudGui::slotSyncStateChange(Folder *folder)
{
    slotComputeOverallSyncStatus();
    updateContextMenuNeeded();

    if (!folder) {
        return; // Valid, just a general GUI redraw was needed.
    }

    auto result = folder->syncResult();

    qCInfo(lcApplication) << "Sync state changed for folder " << folder->remoteUrl().toString() << ": " << result.statusString();

    if (result.status() == SyncResult::NotYetStarted) {
        _settingsDialog->slotRefreshActivity(folder->accountState());
    }
}

void ownCloudGui::slotFoldersChanged()
{
    slotComputeOverallSyncStatus();
    updateContextMenuNeeded();
}

void ownCloudGui::slotOpenPath(const QString &path)
{
    showInFileManager(path);
}

void ownCloudGui::slotAccountStateChanged()
{
    updateContextMenuNeeded();
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
                .arg(account->displayName(), account->capabilities().status().versionString()));
    }
}

void ownCloudGui::slotComputeOverallSyncStatus()
{
    bool allSignedOut = true;
    bool allPaused = true;
    bool allDisconnected = true;
    QVector<AccountStatePtr> problemAccounts;
    auto setStatusText = [&](const QString &text) {
        // Don't overwrite the status if we're currently syncing
        if (FolderMan::instance()->isAnySyncRunning())
            return;
        _actionStatus->setText(text);
    };

    for (const auto &a : AccountManager::instance()->accounts()) {
        if (!a->isSignedOut()) {
            allSignedOut = false;
        }
        if (!a->isConnected()) {
            problemAccounts.append(a);
        } else {
            allDisconnected = false;
        }
    }

    const auto &map = FolderMan::instance()->folders();
    for (auto *f : map) {
        if (!f->syncPaused()) {
            allPaused = false;
        }
    }

    if (!problemAccounts.empty()) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true, contextMenuVisible()));
        if (allDisconnected) {
            setStatusText(tr("Disconnected"));
        } else {
            setStatusText(tr("Disconnected from some accounts"));
        }
#ifdef Q_OS_WIN
        // Windows has a 128-char tray tooltip length limit.
        QStringList accountNames;
        for (const auto &a : qAsConst(problemAccounts)) {
            accountNames.append(a->account()->displayName());
        }
        _tray->setToolTip(tr("Disconnected from %1").arg(accountNames.join(QLatin1String(", "))));
#else
        QStringList messages;
        messages.append(tr("Disconnected from accounts:"));
        for (const auto &a : qAsConst(problemAccounts)) {
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
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true, contextMenuVisible()));
        _tray->setToolTip(tr("Please sign in"));
        setStatusText(tr("Signed out"));
        return;
    } else if (allPaused) {
        _tray->setIcon(Theme::instance()->syncStateIcon(SyncResult::Paused, true, contextMenuVisible()));
        _tray->setToolTip(tr("Account synchronization is disabled"));
        setStatusText(tr("Synchronization is paused"));
        return;
    }

    // display the info of the least successful sync (eg. do not just display the result of the latest sync)
    QString trayMessage;

    auto trayOverallStatusResult = FolderMan::trayOverallStatus(map);

    const QIcon statusIcon = Theme::instance()->syncStateIcon(trayOverallStatusResult.overallStatus(), true, contextMenuVisible());
    _tray->setIcon(statusIcon);

    // create the tray blob message, check if we have an defined state
#ifdef Q_OS_WIN
    // Windows has a 128-char tray tooltip length limit.
    trayMessage = FolderMan::instance()->trayTooltipStatusString(trayOverallStatusResult.overallStatus(), false);
#else
    QStringList allStatusStrings;
    for (auto *folder : map) {
        QString folderMessage = FolderMan::trayTooltipStatusString(
            folder->syncResult(),
            folder->syncPaused());
        allStatusStrings += tr("Folder %1: %2").arg(folder->shortGuiLocalPath(), folderMessage);
    }
    trayMessage = allStatusStrings.join(QLatin1String("\n"));
#endif
    _tray->setToolTip(trayMessage);

    switch (trayOverallStatusResult.overallStatus().status()) {
    case SyncResult::Problem:
        if (trayOverallStatusResult.overallStatus().hasUnresolvedConflicts()) {
            setStatusText(tr("Unresolved %1 conflicts").arg(QString::number(trayOverallStatusResult.overallStatus().numNewConflictItems())));
        } else if (trayOverallStatusResult.overallStatus().numBlacklistErrors() != 0) {
            setStatusText(tr("Ignored errors %1").arg(QString::number(trayOverallStatusResult.overallStatus().numBlacklistErrors())));
        }
        break;
    case SyncResult::Success: {
        QString lastSyncDoneString;
        // display only the time in case the last sync was today
        if (QDateTime::currentDateTime().date() == trayOverallStatusResult.lastSyncDone.date()) {
            lastSyncDoneString = QLocale().toString(trayOverallStatusResult.lastSyncDone.time());
        } else {
            lastSyncDoneString = QLocale().toString(trayOverallStatusResult.lastSyncDone);
        }
        setStatusText(tr("Up to date (%1)").arg(lastSyncDoneString));
    } break;
    case SyncResult::Undefined:
        _tray->setToolTip(tr("There are no sync folders configured."));
        setStatusText(tr("No sync folders configured"));
    default:
        setStatusText(FolderMan::instance()->trayTooltipStatusString(trayOverallStatusResult.overallStatus(), false));
    }
}

void ownCloudGui::addAccountContextMenu(AccountStatePtr accountState, QMenu *menu, bool separateMenu)
{
    // Only show the name in the action if it's not part of an
    // account sub menu.
    QString browserOpen = tr("Open in browser");
    if (!separateMenu) {
        browserOpen = tr("Open %1 in browser").arg(Theme::instance()->appNameGUI());
    }
    auto actionOpenoC = menu->addAction(browserOpen);
    actionOpenoC->setProperty(propertyAccountC, QVariant::fromValue(accountState->account()));
    QObject::connect(actionOpenoC, &QAction::triggered, this, &ownCloudGui::slotOpenOwnCloud);

    FolderMan *folderMan = FolderMan::instance();
    bool firstFolder = true;
    const auto &map = folderMan->folders();
    bool singleSyncFolder = map.size() == 1 && Theme::instance()->singleSyncFolder();
    bool onePaused = false;
    bool allPaused = true;
    for (auto *folder : map) {
        if (folder->accountState() != accountState.data()) {
            continue;
        }

        if (folder->syncPaused()) {
            onePaused = true;
        } else {
            allPaused = false;
        }

        if (firstFolder && !singleSyncFolder) {
            firstFolder = false;
            menu->addSeparator();
            menu->addAction(tr("Managed Folders:"))->setDisabled(true);
        }

        QAction *action = menu->addAction(tr("Open folder '%1'").arg(folder->shortGuiLocalPath()));
        connect(action, &QAction::triggered, this, [this, folder] { this->slotFolderOpenAction(folder); });
    }

    menu->addSeparator();
    if (separateMenu) {
        if (onePaused) {
            QAction *enable = menu->addAction(tr("Unpause all folders"));
            enable->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(enable, &QAction::triggered, this, &ownCloudGui::slotUnpauseAllFolders);
        }
        if (!allPaused) {
            QAction *enable = menu->addAction(tr("Pause all folders"));
            enable->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(enable, &QAction::triggered, this, &ownCloudGui::slotPauseAllFolders);
        }

        if (accountState->isSignedOut()) {
            QAction *signin = menu->addAction(tr("Log in..."));
            signin->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signin, &QAction::triggered, this, &ownCloudGui::slotLogin);
        } else {
            QAction *signout = menu->addAction(tr("Log out"));
            signout->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signout, &QAction::triggered, this, &ownCloudGui::slotLogout);
        }
    }
}

SettingsDialog *ownCloudGui::settingsDialog() const
{
    return _settingsDialog;
}

void ownCloudGui::slotContextMenuAboutToShow()
{
    _contextMenuVisibleManual = true;

    // Update icon in sys tray, as it might change depending on the context menu state
    slotComputeOverallSyncStatus();

    if (!_workaroundNoAboutToShowUpdate) {
        updateContextMenu();
    }
}

void ownCloudGui::slotContextMenuAboutToHide()
{
    _contextMenuVisibleManual = false;

    // Update icon in sys tray, as it might change depending on the context menu state
    slotComputeOverallSyncStatus();
}

bool ownCloudGui::contextMenuVisible() const
{
    // On some platforms isVisible doesn't work and always returns false,
    // elsewhere aboutToHide is unreliable.
    if (_workaroundManualVisibility)
        return _contextMenuVisibleManual;
    return _contextMenu->isVisible();
}

void ownCloudGui::hideAndShowTray()
{
    _tray->hide();
    _tray->show();
}

static bool minimalTrayMenu()
{
    static QByteArray var = qgetenv("OWNCLOUD_MINIMAL_TRAY_MENU");
    return !var.isEmpty();
}

static bool updateWhileVisible()
{
    static QByteArray var = qgetenv("OWNCLOUD_TRAY_UPDATE_WHILE_VISIBLE");
    if (var == "1") {
        return true;
    } else if (var == "0") {
        return false;
    } else {
        // triggers bug on OS X: https://bugreports.qt.io/browse/QTBUG-54845
        // or flickering on Xubuntu
        return false;
    }
}

static QByteArray envForceQDBusTrayWorkaround()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_QDBUS_TRAY_WORKAROUND");
    return var;
}

static QByteArray envForceWorkaroundShowAndHideTray()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_TRAY_SHOW_HIDE");
    return var;
}

static QByteArray envForceWorkaroundNoAboutToShowUpdate()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_TRAY_NO_ABOUT_TO_SHOW");
    return var;
}

static QByteArray envForceWorkaroundFakeDoubleClick()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_TRAY_FAKE_DOUBLE_CLICK");
    return var;
}

static QByteArray envForceWorkaroundManualVisibility()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_TRAY_MANUAL_VISIBILITY");
    return var;
}

void ownCloudGui::setupContextMenu()
{
    if (_contextMenu) {
        return;
    }

    _contextMenu.reset(new QMenu());
    _contextMenu->setTitle(Theme::instance()->appNameGUI());

    _recentActionsMenu = new QMenu(tr("Recent Changes"), _contextMenu.data());

    // this must be called only once after creating the context menu, or
    // it will trigger a bug in Ubuntu's SNI bridge patch (11.10, 12.04).
    _tray->setContextMenu(_contextMenu.data());

    // The tray menu is surprisingly problematic. Being able to switch to
    // a minimal version of it is a useful workaround and testing tool.
    if (minimalTrayMenu()) {
        if (! Theme::instance()->about().isEmpty()) {
            _contextMenu->addSeparator();
            _contextMenu->addAction(_actionAbout);
        }
        _contextMenu->addAction(_actionQuit);
        return;
    }

    auto applyEnvVariable = [](bool *sw, const QByteArray &value) {
        if (value == "1")
            *sw = true;
        if (value == "0")
            *sw = false;
    };

    // This is an old compound flag that people might still depend on
    bool qdbusmenuWorkarounds = false;
    applyEnvVariable(&qdbusmenuWorkarounds, envForceQDBusTrayWorkaround());
    if (qdbusmenuWorkarounds) {
        _workaroundFakeDoubleClick = true;
        _workaroundNoAboutToShowUpdate = true;
        _workaroundShowAndHideTray = true;
    }

#ifdef Q_OS_MAC
    // https://bugreports.qt.io/browse/QTBUG-54633
    _workaroundNoAboutToShowUpdate = true;
    _workaroundManualVisibility = true;
#endif

#ifdef Q_OS_LINUX
    // For KDE sessions if the platform plugin is missing,
    // neither aboutToShow() updates nor the isVisible() call
    // work. At least aboutToHide is reliable.
    // https://github.com/owncloud/client/issues/6545
    static QByteArray xdgCurrentDesktop = qgetenv("XDG_CURRENT_DESKTOP");
    static QByteArray desktopSession = qgetenv("DESKTOP_SESSION");
    bool isKde =
        xdgCurrentDesktop.contains("KDE")
        || desktopSession.contains("plasma")
        || desktopSession.contains("kde");
    QObject *platformMenu = reinterpret_cast<QObject *>(_tray->contextMenu()->platformMenu());
    if (isKde && platformMenu && platformMenu->metaObject()->className() == QByteArrayLiteral("QDBusPlatformMenu")) {
        _workaroundManualVisibility = true;
        _workaroundNoAboutToShowUpdate = true;
    }
#endif

    applyEnvVariable(&_workaroundNoAboutToShowUpdate, envForceWorkaroundNoAboutToShowUpdate());
    applyEnvVariable(&_workaroundFakeDoubleClick, envForceWorkaroundFakeDoubleClick());
    applyEnvVariable(&_workaroundShowAndHideTray, envForceWorkaroundShowAndHideTray());
    applyEnvVariable(&_workaroundManualVisibility, envForceWorkaroundManualVisibility());

    qCInfo(lcApplication) << "Tray menu workarounds:"
                          << "noabouttoshow:" << _workaroundNoAboutToShowUpdate
                          << "fakedoubleclick:" << _workaroundFakeDoubleClick
                          << "showhide:" << _workaroundShowAndHideTray
                          << "manualvisibility:" << _workaroundManualVisibility;


    connect(&_delayedTrayUpdateTimer, &QTimer::timeout, this, &ownCloudGui::updateContextMenu);
    _delayedTrayUpdateTimer.setInterval(2s);
    _delayedTrayUpdateTimer.setSingleShot(true);

    connect(_contextMenu.data(), &QMenu::aboutToShow, this, &ownCloudGui::slotContextMenuAboutToShow);
    // unfortunately aboutToHide is unreliable, it seems to work on OSX though
    connect(_contextMenu.data(), &QMenu::aboutToHide, this, &ownCloudGui::slotContextMenuAboutToHide);

    // Populate the context menu now.
    updateContextMenu();
}

void ownCloudGui::updateContextMenu()
{
    if (minimalTrayMenu()) {
        return;
    }

    // If it's visible, we can't update live, and it won't be updated lazily: reschedule
    if (contextMenuVisible() && !updateWhileVisible() && _workaroundNoAboutToShowUpdate) {
        if (!_delayedTrayUpdateTimer.isActive()) {
            _delayedTrayUpdateTimer.start();
        }
        return;
    }

    if (_workaroundShowAndHideTray) {
        // To make tray menu updates work with these bugs (see setupContextMenu)
        // we need to hide and show the tray icon. We don't want to do that
        // while it's visible!
        if (contextMenuVisible()) {
            if (!_delayedTrayUpdateTimer.isActive()) {
                _delayedTrayUpdateTimer.start();
            }
            return;
        }
        _tray->hide();
    }

    _contextMenu->clear();
    slotRebuildRecentMenus();

    // We must call deleteLater because we might be called from the press in one of the actions.
    for (auto *menu : qAsConst(_accountMenus)) {
        menu->deleteLater();
    }
    _accountMenus.clear();


    const auto &accountList = AccountManager::instance()->accounts();

    bool isConfigured = (!accountList.isEmpty());
    bool atLeastOneConnected = false;
    bool atLeastOneSignedOut = false;
    bool atLeastOneSignedIn = false;
    bool atLeastOnePaused = false;
    bool atLeastOneNotPaused = false;
    for (const auto &a : accountList) {
        if (a->isConnected()) {
            atLeastOneConnected = true;
        }
        if (a->isSignedOut()) {
            atLeastOneSignedOut = true;
        } else {
            atLeastOneSignedIn = true;
        }
    }

    for (auto *f : FolderMan::instance()->folders()) {
        if (f->syncPaused()) {
            atLeastOnePaused = true;
        } else {
            atLeastOneNotPaused = true;
        }
    }

    if (accountList.count() > 1) {
        for (const auto &account : accountList) {
            QMenu *accountMenu = new QMenu(account->account()->displayName(), _contextMenu.data());
            _accountMenus.append(accountMenu);
            _contextMenu->addMenu(accountMenu);

            addAccountContextMenu(account, accountMenu, true);
        }
    } else if (accountList.count() == 1) {
        addAccountContextMenu(accountList.first(), _contextMenu.data(), false);
    }

    _contextMenu->addSeparator();

    _contextMenu->addAction(_actionStatus);
    if (isConfigured && atLeastOneConnected) {
        _contextMenu->addMenu(_recentActionsMenu);
    }

    _contextMenu->addSeparator();

    if (accountList.isEmpty()) {
        _contextMenu->addAction(_actionNewAccountWizard);
    }
    _contextMenu->addAction(_actionSettings);
    if (!Theme::instance()->helpUrl().isEmpty()) {
        _contextMenu->addAction(_actionHelp);
    }

    if (_actionCrash) {
        _contextMenu->addAction(_actionCrash);
        _contextMenu->addAction(_actionCrashEnforce);
        _contextMenu->addAction(_actionCrashFatal);

    }

    _contextMenu->addSeparator();
    if (atLeastOnePaused) {
        QString text;
        if (accountList.count() > 1) {
            text = tr("Unpause all synchronization");
        } else {
            text = tr("Unpause synchronization");
        }
        QAction *action = _contextMenu->addAction(text);
        connect(action, &QAction::triggered, this, &ownCloudGui::slotUnpauseAllFolders);
    }
    if (atLeastOneNotPaused) {
        QString text;
        if (accountList.count() > 1) {
            text = tr("Pause all synchronization");
        } else {
            text = tr("Pause synchronization");
        }
        QAction *action = _contextMenu->addAction(text);
        connect(action, &QAction::triggered, this, &ownCloudGui::slotPauseAllFolders);
    }
    if (atLeastOneSignedIn) {
        if (accountList.count() > 1) {
            _actionLogout->setText(tr("Log out of all accounts"));
        } else {
            _actionLogout->setText(tr("Log out"));
        }
        _contextMenu->addAction(_actionLogout);
    }
    if (atLeastOneSignedOut) {
        if (accountList.count() > 1) {
            _actionLogin->setText(tr("Log in to all accounts..."));
        } else {
            _actionLogin->setText(tr("Log in..."));
        }
        _contextMenu->addAction(_actionLogin);
    }

    if (! Theme::instance()->about().isEmpty()) {
        _contextMenu->addSeparator();
        _contextMenu->addAction(_actionAbout);
    }

    _contextMenu->addAction(_actionQuit);

    if (_workaroundShowAndHideTray) {
        _tray->show();
    }
}

void ownCloudGui::updateContextMenuNeeded()
{
    // if it's visible and we can update live: update now
    if (contextMenuVisible() && updateWhileVisible()) {
        // Note: don't update while visible on OSX
        // https://bugreports.qt.io/browse/QTBUG-54845
        updateContextMenu();
        return;
    }

    // if we can't lazily update: update later
    if (_workaroundNoAboutToShowUpdate) {
        // Note: don't update immediately even in the invisible case
        // as that can lead to extremely frequent menu updates
        if (!_delayedTrayUpdateTimer.isActive()) {
            _delayedTrayUpdateTimer.start();
        }
        return;
    }
}

void ownCloudGui::slotShowTrayMessage(const QString &title, const QString &msg, const QIcon &icon)
{
    // SyncResult::Problem is returns the info icon
    _tray->showMessage(title, msg, icon.isNull() ? Theme::instance()->syncStateIcon(SyncResult::Problem) : icon);
}

void ownCloudGui::slotShowOptionalTrayMessage(const QString &title, const QString &msg, const QIcon &icon)
{
    ConfigFile cfg;
    if (cfg.optionalDesktopNotifications()) {
        slotShowTrayMessage(title, msg, icon);
    }
}


/*
 * open the folder with the given Alias
 */
void ownCloudGui::slotFolderOpenAction(Folder *f)
{
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

void ownCloudGui::setupActions()
{
    _actionStatus = new QAction(tr("Unknown status"), this);
    _actionStatus->setEnabled(false);
    _actionSettings = new QAction(tr("Show %1").arg(Theme::instance()->appNameGUI()), this);
    _actionNewAccountWizard = new QAction(tr("New account..."), this);
    _actionRecent = new QAction(tr("Details..."), this);
    _actionRecent->setEnabled(true);

    QObject::connect(_actionRecent, &QAction::triggered, this, &ownCloudGui::slotShowSyncProtocol);
    QObject::connect(_actionSettings, &QAction::triggered, this, &ownCloudGui::slotShowSettings);
    QObject::connect(_actionNewAccountWizard, &QAction::triggered, this, &ownCloudGui::runNewAccountWizard);
    _actionHelp = new QAction(tr("Help"), this);
    QObject::connect(_actionHelp, &QAction::triggered, this, &ownCloudGui::slotHelp);
    _actionAbout = new QAction(tr("About %1").arg(Theme::instance()->appNameGUI()), this);
    QObject::connect(_actionAbout, &QAction::triggered, this, &ownCloudGui::slotAbout);
    _actionQuit = new QAction(tr("Quit %1").arg(Theme::instance()->appNameGUI()), this);
    QObject::connect(_actionQuit, &QAction::triggered, _app, &QApplication::quit);

    _actionLogin = new QAction(tr("Log in..."), this);
    connect(_actionLogin, &QAction::triggered, this, &ownCloudGui::slotLogin);
    _actionLogout = new QAction(tr("Log out"), this);
    connect(_actionLogout, &QAction::triggered, this, &ownCloudGui::slotLogout);

    if (_app->debugMode()) {
        _actionCrash = new QAction(QStringLiteral("Crash now - Div by zero"), this);
        connect(_actionCrash, &QAction::triggered, _app, &Application::slotCrash);
        _actionCrashEnforce = new QAction(QStringLiteral("Crash now - ENFORCE()"), this);
        connect(_actionCrashEnforce, &QAction::triggered, _app, &Application::slotCrashEnforce);
        _actionCrashFatal = new QAction(QStringLiteral("Crash now - qFatal"), this);
        connect(_actionCrashFatal, &QAction::triggered, _app, &Application::slotCrashFatal);
    } else {
        _actionCrash = nullptr;
        _actionCrashEnforce = nullptr;
        _actionCrashFatal = nullptr;
    }
}

void ownCloudGui::slotRebuildRecentMenus()
{
    _recentActionsMenu->clear();
    if (!_recentItemsActions.isEmpty()) {
        for (auto *a : qAsConst(_recentItemsActions)) {
            _recentActionsMenu->addAction(a);
        }
        _recentActionsMenu->addSeparator();
    } else {
        _recentActionsMenu->addAction(tr("No items synced recently"))->setEnabled(false);
    }
    // add a more... entry.
    _recentActionsMenu->addAction(_actionRecent);
}

/// Returns true if the completion of a given item should show up in the
/// 'Recent Activity' menu
static bool shouldShowInRecentsMenu(const SyncFileItem &item)
{
    return !Progress::isIgnoredKind(item._status)
        && item._instruction != CSYNC_INSTRUCTION_EVAL
        && item._instruction != CSYNC_INSTRUCTION_NONE;
}

void ownCloudGui::slotUpdateProgress(Folder *folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Discovery) {
        if (!progress._currentDiscoveredRemoteFolder.isEmpty()) {
            _actionStatus->setText(tr("Checking for changes in remote '%1'")
                                       .arg(progress._currentDiscoveredRemoteFolder));
        } else if (!progress._currentDiscoveredLocalFolder.isEmpty()) {
            _actionStatus->setText(tr("Checking for changes in local '%1'")
                                       .arg(progress._currentDiscoveredLocalFolder));
        }
    } else if (progress.status() == ProgressInfo::Done) {
        QTimer::singleShot(2s, this, &ownCloudGui::slotComputeOverallSyncStatus);
    }
    if (progress.status() != ProgressInfo::Propagation) {
        return;
    }

    if (progress.totalSize() == 0) {
        qint64 currentFile = progress.currentFile();
        qint64 totalFileCount = qMax(progress.totalFiles(), currentFile);
        QString msg;
        if (progress.trustEta()) {
            msg = tr("Syncing %1 of %2  (%3 left)")
                      .arg(currentFile)
                      .arg(totalFileCount)
                      .arg(Utility::durationToDescriptiveString2(progress.totalProgress().estimatedEta));
        } else {
            msg = tr("Syncing %1 of %2")
                      .arg(currentFile)
                      .arg(totalFileCount);
        }
        _actionStatus->setText(msg);
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
        _actionStatus->setText(msg);
    }

    _actionRecent->setIcon(QIcon()); // Fixme: Set a "in-progress"-item eventually.

    if (!progress._lastCompletedItem.isEmpty()
        && shouldShowInRecentsMenu(progress._lastCompletedItem)) {
        if (Progress::isWarningKind(progress._lastCompletedItem._status)) {
            // display a warn icon if warnings happened.
            _actionRecent->setIcon(Utility::getCoreIcon(QStringLiteral("warning")));
        }

        QString kindStr = Progress::asResultString(progress._lastCompletedItem);
        QString timeStr = QTime::currentTime().toString(QStringLiteral("hh:mm"));
        QString actionText = tr("%1 (%2, %3)").arg(progress._lastCompletedItem._file, kindStr, timeStr);
        QAction *action = new QAction(actionText, this);
        QString fullPath = folder->path() + QLatin1Char('/') + progress._lastCompletedItem._file;
        if (QFile(fullPath).exists()) {
            connect(action, &QAction::triggered, this, [this, fullPath] { this->slotOpenPath(fullPath); });
        } else {
            action->setEnabled(false);
        }
        if (_recentItemsActions.length() > 5) {
            _recentItemsActions.takeFirst()->deleteLater();
        }
        _recentItemsActions.append(action);

        // Update the "Recent" menu if the context menu is being shown,
        // otherwise it'll be updated later, when the context menu is opened.
        if (updateWhileVisible() && contextMenuVisible()) {
            slotRebuildRecentMenus();
        }
    }
}

void ownCloudGui::slotLogin()
{
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        account->signIn();
    } else {
        for (const auto &a : AccountManager::instance()->accounts()) {
            a->signIn();
        }
    }
}

void ownCloudGui::slotLogout()
{
    auto list = AccountManager::instance()->accounts();
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        list.clear();
        list.insert(account->account()->uuid(), account);
    }

    for (const auto &ai : qAsConst(list)) {
        ai->signOutByUi();
    }
}

void ownCloudGui::slotUnpauseAllFolders()
{
    setPauseOnAllFoldersHelper(false);
}

void ownCloudGui::slotPauseAllFolders()
{
    setPauseOnAllFoldersHelper(true);
}

void ownCloudGui::runNewAccountWizard()
{
    if (_wizardController.isNull()) {
        // passing the settings dialog as parent makes sure the wizard will be shown above it
        // as the settingsDialog's lifetime spans across the entire application but the dialog will live much shorter,
        // we have to clean it up manually when finished() is emitted
        _wizardController = new Wizard::SetupWizardController(settingsDialog());

        // while the wizard is shown, new syncs are disabled
        FolderMan::instance()->setSyncEnabled(false);

        connect(_wizardController, &Wizard::SetupWizardController::finished, ocApp(),
            [this](AccountPtr newAccount, Wizard::SyncMode syncMode) {
                // note: while the wizard is shown, we disable the folder synchronization
                // previously we could perform this just here, but now we have to postpone this depending on whether selective sync was chosen
                // see also #9497

                // when the dialog is closed before it has finished, there won't be a new account to set up
                // the wizard controller signalizes this by passing a null pointer
                if (!newAccount.isNull()) {
                    // finally, call the slot that finalizes the setup
                    auto accountStatePtr = ocApp()->addNewAccount(newAccount);

                    // ensure we are connected and fetch the capabilities
                    auto validator = new ConnectionValidator(accountStatePtr->account(), accountStatePtr->account().data());

                    QObject::connect(validator, &ConnectionValidator::connectionResult, accountStatePtr.data(), [accountStatePtr, syncMode](ConnectionValidator::Status status, const QStringList &errors) {
                        if (OC_ENSURE(status == ConnectionValidator::Connected || status == ConnectionValidator::ServerVersionMismatch)) {
                            // saving once after adding makes sure the account is stored in the config in a working state
                            // this is needed to ensure a consistent state in the config file upon unexpected terminations of the client
                            // (for instance, when running from a debugger and stopping the process from there)
                            AccountManager::instance()->save(true);

                            switch (syncMode) {
                            case Wizard::SyncMode::SyncEverything:
                            case Wizard::SyncMode::UseVfs: {
                                bool useVfs = syncMode == Wizard::SyncMode::UseVfs;
                                setUpInitialSyncFolder(accountStatePtr, useVfs);

                                break;
                            }
                            case Wizard::SyncMode::ConfigureUsingFolderWizard: {
                                Q_ASSERT(!accountStatePtr->account()->hasDefaultSyncRoot());

                                auto *folderWizard = new FolderWizard(accountStatePtr, ocApp()->gui()->settingsDialog());
                                folderWizard->resize(ocApp()->gui()->settingsDialog()->sizeHintForChild());
                                folderWizard->setAttribute(Qt::WA_DeleteOnClose);

                                // TODO: duplication of AccountSettings
                                // adapted from AccountSettings::slotFolderWizardAccepted()
                                connect(folderWizard, &QDialog::accepted, [accountStatePtr, folderWizard]() {
                                    FolderMan *folderMan = FolderMan::instance();

                                    qCInfo(lcApplication) << "Folder wizard completed";
                                    const auto config = folderWizard->result();

                                    auto folder = folderMan->addFolderFromFolderWizardResult(accountStatePtr, config);

                                    if (!config.selectiveSyncBlackList.isEmpty() && OC_ENSURE(folder && !config.useVirtualFiles)) {
                                        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, config.selectiveSyncBlackList);

                                        // The user already accepted the selective sync dialog. everything is in the white list
                                        folder->journalDb()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList, { QLatin1String("/") });
                                    }

                                    folderMan->setSyncEnabled(true);
                                    folderMan->scheduleAllFolders();
                                });

                                connect(folderWizard, &QDialog::rejected, []() {
                                    qCInfo(lcApplication) << "Folder wizard cancelled";
                                    FolderMan::instance()->setSyncEnabled(true);
                                });

                                folderWizard->open();
                                ocApp()->gui()->raiseDialog(folderWizard);

                                break;
                            }
                            default:
                                Q_UNREACHABLE();
                            }
                        }
                    });


                    validator->checkServer();
                } else {
                    FolderMan::instance()->setSyncEnabled(true);
                }

                // make sure the wizard is cleaned up eventually
                _wizardController->deleteLater();
            });

        // all we have to do is show the dialog...
        _wizardController->window()->show();
        // ... and bring it to the front
        raiseDialog(_wizardController->window());
    }
}

void ownCloudGui::setPauseOnAllFoldersHelper(bool pause)
{
    QList<AccountStatePtr> accounts;
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        accounts.append(account);
    } else {
        for (const auto &a : AccountManager::instance()->accounts()) {
            accounts.append(a);
        }
    }
    for (auto *f : FolderMan::instance()->folders()) {
        if (accounts.contains(f->accountState())) {
            f->setSyncPaused(pause);
            if (pause) {
                f->slotTerminateSync();
            }
        }
    }
}

void ownCloudGui::slotShowGuiMessage(const QString &title, const QString &message)
{
    QMessageBox *msgBox = new QMessageBox(settingsDialog());
    msgBox->setWindowFlags(msgBox->windowFlags() | Qt::WindowStaysOnTopHint);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setText(message);
    msgBox->setWindowTitle(title);
    msgBox->setIcon(QMessageBox::Information);
    msgBox->open();
    raiseDialog(msgBox);
}

void ownCloudGui::slotShowSettings()
{
    raiseDialog(_settingsDialog);
}

void ownCloudGui::slotShowSyncProtocol()
{
    slotShowSettings();
    _settingsDialog->showActivityPage();
}


void ownCloudGui::slotShutdown()
{
    // explicitly close windows. This is somewhat of a hack to ensure
    // that saving the geometries happens ASAP during a OS shutdown

    // those do delete on close
    _settingsDialog->close();
}

void ownCloudGui::slotToggleLogBrowser()
{
    auto logBrowser = new LogBrowser(settingsDialog());
    logBrowser->setAttribute(Qt::WA_DeleteOnClose);
    logBrowser->open();
    raiseDialog(logBrowser);
}

void ownCloudGui::slotOpenOwnCloud()
{
    if (auto account = qvariant_cast<AccountPtr>(sender()->property(propertyAccountC))) {
        QDesktopServices::openUrl(account->url());
    }
}

void ownCloudGui::slotHelp()
{
    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void ownCloudGui::raiseDialog(QWidget *raiseWidget)
{
    auto window = ocApp()->gui()->settingsDialog();
    OC_ASSERT(window);
    OC_ASSERT_X(!qobject_cast<QDialog *>(raiseWidget) || raiseWidget->parentWidget() == window, "raiseDialog should only be called with modal dialogs");
    if (!window) {
        return;
    }
    window->showNormal();
    window->raise();
    raiseWidget->showNormal();
    raiseWidget->raise();
    window->activateWindow();
    raiseWidget->activateWindow();

#if defined(Q_OS_WIN)
    // Windows disallows raising a Window when you're not the active application.
    // Use a common hack to attach to the active application
    const auto activeProcessId = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    if (activeProcessId != qApp->applicationPid()) {
        const auto threadId = GetCurrentThreadId();
        // don't step here with a debugger...
        if (AttachThreadInput(threadId, activeProcessId, true))
        {
            const auto hwnd = reinterpret_cast<HWND>(window->winId());
            SetForegroundWindow(hwnd);
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            AttachThreadInput(threadId, activeProcessId, false);
        }
    }
#endif
}


void ownCloudGui::slotShowShareDialog(const QString &sharePath, const QString &localPath, ShareDialogStartPage startPage)
{
    QString file;
    const auto folder = FolderMan::instance()->folderForPath(localPath, &file);
    if (!folder) {
        qCWarning(lcApplication) << "Could not open share dialog for" << localPath << "no responsible folder found";
        return;
    }

    const auto accountState = folder->accountState();

    SyncJournalFileRecord fileRecord;

    bool resharingAllowed = true; // lets assume the good
    if (folder->journalDb()->getFileRecord(file, &fileRecord) && fileRecord.isValid()) {
        // check the permission: Is resharing allowed?
        if (!fileRecord._remotePerm.isNull() && !fileRecord._remotePerm.hasPermission(RemotePermissions::CanReshare)) {
            resharingAllowed = false;
        }
    }

    // As a first approximation, set the set of permissions that can be granted
    // either to everything (resharing allowed) or nothing (no resharing).
    //
    // The correct value will be found with a propfind from ShareDialog.
    // (we want to show the dialog directly, not wait for the propfind first)
    SharePermissions maxSharingPermissions =
        SharePermissionRead
        | SharePermissionUpdate | SharePermissionCreate | SharePermissionDelete
        | SharePermissionShare;
    if (!resharingAllowed) {
        maxSharingPermissions = SharePermission(0);
    }


    ShareDialog *w = nullptr;
    if (_shareDialogs.contains(localPath) && _shareDialogs[localPath]) {
        qCInfo(lcApplication) << "Raising share dialog" << sharePath << localPath;
        w = _shareDialogs[localPath];
    } else {
        qCInfo(lcApplication) << "Opening share dialog" << sharePath << localPath << maxSharingPermissions;
        w = new ShareDialog(accountState, folder->webDavUrl(), sharePath, localPath, maxSharingPermissions, startPage, settingsDialog());
        w->setAttribute(Qt::WA_DeleteOnClose, true);

        _shareDialogs[localPath] = w;
        connect(w, &QObject::destroyed, this, &ownCloudGui::slotRemoveDestroyedShareDialogs);
    }
    w->open();
    raiseDialog(w);
}

void ownCloudGui::slotRemoveDestroyedShareDialogs()
{
    QMutableMapIterator<QString, QPointer<ShareDialog>> it(_shareDialogs);
    while (it.hasNext()) {
        it.next();
        if (!it.value() || it.value() == sender()) {
            it.remove();
        }
    }
}

void ownCloudGui::slotAbout()
{
    if(!_aboutDialog) {
        _aboutDialog = new AboutDialog(_settingsDialog);
        _aboutDialog->setAttribute(Qt::WA_DeleteOnClose);
        _aboutDialog->open();
    }
    raiseDialog(_aboutDialog);
}


} // end namespace
