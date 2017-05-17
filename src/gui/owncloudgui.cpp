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

#include "application.h"
#include "owncloudgui.h"
#include "theme.h"
#include "folderman.h"
#include "configfile.h"
#include "utility.h"
#include "progressdispatcher.h"
#include "owncloudsetupwizard.h"
#include "sharedialog.h"
#if defined(Q_OS_MAC)
#include "settingsdialogmac.h"
#include "macwindow.h" // qtmacgoodies
#else
#include "settingsdialog.h"
#endif
#include "logger.h"
#include "logbrowser.h"
#include "account.h"
#include "accountstate.h"
#include "openfilemanager.h"
#include "accountmanager.h"
#include "creds/abstractcredentials.h"

#include <QDesktopServices>
#include <QDir>
#include <QMessageBox>
#include <QSignalMapper>

#if defined(Q_OS_X11)
#include <QX11Info>
#endif

namespace OCC {

const char propertyAccountC[] = "oc_account";

ownCloudGui::ownCloudGui(Application *parent)
    : QObject(parent)
    , _tray(0)
    ,
#if defined(Q_OS_MAC)
    _settingsDialog(new SettingsDialogMac(this))
    ,
#else
    _settingsDialog(new SettingsDialog(this))
    ,
#endif
    _logBrowser(0)
    , _contextMenuVisibleOsx(false)
    , _recentActionsMenu(0)
    , _qdbusmenuWorkaround(false)
    , _folderOpenActionMapper(new QSignalMapper(this))
    , _recentItemsMapper(new QSignalMapper(this))
    , _app(parent)
{
    _tray = new Systray();
    _tray->setParent(this);

    // for the beginning, set the offline icon until the account was verified
    _tray->setIcon(Theme::instance()->folderOfflineIcon(/*systray?*/ true, /*currently visible?*/ false));

    connect(_tray.data(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        SLOT(slotTrayClicked(QSystemTrayIcon::ActivationReason)));

    setupActions();
    setupContextMenu();

    _tray->show();

    /* use a signal mapper to map the open requests to the alias names */
    connect(_folderOpenActionMapper, SIGNAL(mapped(QString)),
        this, SLOT(slotFolderOpenAction(QString)));

    connect(_recentItemsMapper, SIGNAL(mapped(QString)),
        this, SLOT(slotOpenPath(QString)));

    ProgressDispatcher *pd = ProgressDispatcher::instance();
    connect(pd, SIGNAL(progressInfo(QString, ProgressInfo)), this,
        SLOT(slotUpdateProgress(QString, ProgressInfo)));

    FolderMan *folderMan = FolderMan::instance();
    connect(folderMan, SIGNAL(folderSyncStateChange(Folder *)),
        this, SLOT(slotSyncStateChange(Folder *)));

    connect(AccountManager::instance(), SIGNAL(accountAdded(AccountState *)),
        SLOT(updateContextMenuNeeded()));
    connect(AccountManager::instance(), SIGNAL(accountRemoved(AccountState *)),
        SLOT(updateContextMenuNeeded()));

    connect(Logger::instance(), SIGNAL(guiLog(QString, QString)),
        SLOT(slotShowTrayMessage(QString, QString)));
    connect(Logger::instance(), SIGNAL(optionalGuiLog(QString, QString)),
        SLOT(slotShowOptionalTrayMessage(QString, QString)));
    connect(Logger::instance(), SIGNAL(guiMessage(QString, QString)),
        SLOT(slotShowGuiMessage(QString, QString)));

    setupOverlayIcons();
}

// Use this to do platform specific code to make overlay icons appear
// in the gui
// MacOSX: perform a AppleScript code piece to load the Finder Plugin.


void ownCloudGui::setupOverlayIcons()
{
#ifdef Q_OS_MAC
    // Make sure that we only send the load event to the legacy plugin when
    // using OS X <= 10.9 since 10.10 starts using the new FinderSync one.
    if (QSysInfo::MacintoshVersion < QSysInfo::MV_10_10) {
        const QLatin1String finderExtension("/Library/ScriptingAdditions/SyncStateFinder.osax");
        if (QFile::exists(finderExtension)) {
            QString aScript = QString::fromUtf8("tell application \"Finder\"\n"
                                                "  try\n"
                                                "    «event OWNCload»\n"
                                                "  end try\n"
                                                "end tell\n");

            QString osascript = "/usr/bin/osascript";
            QStringList processArguments;
            // processArguments << "-l" << "AppleScript";

            QProcess p;
            p.start(osascript, processArguments);
            p.write(aScript.toUtf8());
            p.closeWriteChannel();
            //p.waitForReadyRead(-1);
            p.waitForFinished(5000);
            QByteArray result = p.readAll();
            QString resultAsString(result); // if appropriate
            qCInfo(lcApplication) << "Load Finder Overlay-Plugin: " << resultAsString << ": " << p.exitCode()
                                  << (p.exitCode() != 0 ? p.errorString() : QString::null);
        } else {
            qCWarning(lcApplication) << finderExtension << "does not exist! Finder Overlay Plugin loading failed";
        }
    }
#endif
}

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

void ownCloudGui::slotTrayClicked(QSystemTrayIcon::ActivationReason reason)
{
    if (_qdbusmenuWorkaround) {
        static QElapsedTimer last_click;
        if (last_click.isValid() && last_click.elapsed() < 200) {
            return;
        }
        last_click.start();
    }

    // Left click
    if (reason == QSystemTrayIcon::Trigger) {
        if (OwncloudSetupWizard::bringWizardToFrontIfVisible()) {
            // brought wizard to front
        } else if (_shareDialogs.size() > 0) {
            // Share dialog(s) be hidden by other apps, bring them back
            Q_FOREACH (const QPointer<ShareDialog> &shareDialog, _shareDialogs) {
                Q_ASSERT(shareDialog.data());
                raiseDialog(shareDialog);
            }
        } else {
#ifdef Q_OS_MAC
            // on macOS, a left click always opens menu.
            // However if the settings dialog is already visible but hidden
            // by other applications, this will bring it to the front.
            if (!_settingsDialog.isNull() && _settingsDialog->isVisible()) {
                raiseDialog(_settingsDialog.data());
            }
#else
            slotOpenSettingsDialog();
#endif
        }
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

    if (result.status() == SyncResult::Success || result.status() == SyncResult::Error) {
        Logger::instance()->enterNextLogFile();
    }

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
            tr("The server on account %1 runs an old and unsupported version %2. "
               "Using this client with unsupported server versions is untested and "
               "potentially dangerous. Proceed at your own risk.")
                .arg(account->displayName(), account->serverVersion()));
    }
}

void ownCloudGui::slotComputeOverallSyncStatus()
{
    bool allSignedOut = true;
    bool allPaused = true;
    QVector<AccountStatePtr> problemAccounts;
    foreach (auto a, AccountManager::instance()->accounts()) {
        if (!a->isSignedOut()) {
            allSignedOut = false;
        }
        if (!a->isConnected()) {
            problemAccounts.append(a);
        }
    }
    foreach (Folder *f, FolderMan::instance()->map()) {
        if (!f->syncPaused()) {
            allPaused = false;
        }
    }

    if (!problemAccounts.empty()) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true, contextMenuVisible()));
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
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true, contextMenuVisible()));
        _tray->setToolTip(tr("Please sign in"));
        return;
    } else if (allPaused) {
        _tray->setIcon(Theme::instance()->syncStateIcon(SyncResult::Paused, true, contextMenuVisible()));
        _tray->setToolTip(tr("Account synchronization is disabled"));
        return;
    }

    // display the info of the least successful sync (eg. do not just display the result of the latest sync)
    QString trayMessage;
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map map = folderMan->map();
    SyncResult overallResult = FolderMan::accountStatus(map.values());

    // create the tray blob message, check if we have an defined state
    if (overallResult.status() != SyncResult::Undefined) {
        if (map.count() > 0) {
#ifdef Q_OS_WIN
            // Windows has a 128-char tray tooltip length limit.
            trayMessage = folderMan->statusToString(overallResult.status(), false);
#else
            QStringList allStatusStrings;
            foreach (Folder *folder, map.values()) {
                QString folderMessage = folderMan->statusToString(folder->syncResult().status(), folder->syncPaused());
                allStatusStrings += tr("Folder %1: %2").arg(folder->shortGuiLocalPath(), folderMessage);
            }
            trayMessage = allStatusStrings.join(QLatin1String("\n"));
#endif
        } else {
            trayMessage = tr("No sync folders configured.");
        }

        QIcon statusIcon = Theme::instance()->syncStateIcon(overallResult.status(), true, contextMenuVisible());
        _tray->setIcon(statusIcon);
        _tray->setToolTip(trayMessage);
    } else {
        // undefined because there are no folders.
        QIcon icon = Theme::instance()->syncStateIcon(SyncResult::Problem, true, contextMenuVisible());
        _tray->setIcon(icon);
        _tray->setToolTip(tr("There are no sync folders configured."));
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
    QObject::connect(actionOpenoC, SIGNAL(triggered(bool)), SLOT(slotOpenOwnCloud()));

    FolderMan *folderMan = FolderMan::instance();
    bool firstFolder = true;
    bool singleSyncFolder = folderMan->map().size() == 1 && Theme::instance()->singleSyncFolder();
    bool onePaused = false;
    bool allPaused = true;
    foreach (Folder *folder, folderMan->map()) {
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

        QAction *action = new QAction(tr("Open folder '%1'").arg(folder->shortGuiLocalPath()), menu);
        connect(action, SIGNAL(triggered()), _folderOpenActionMapper, SLOT(map()));
        _folderOpenActionMapper->setMapping(action, folder->alias());
        menu->addAction(action);
    }

    menu->addSeparator();
    if (separateMenu) {
        if (onePaused) {
            QAction *enable = menu->addAction(tr("Unpause all folders"));
            enable->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(enable, SIGNAL(triggered(bool)), SLOT(slotUnpauseAllFolders()));
        }
        if (!allPaused) {
            QAction *enable = menu->addAction(tr("Pause all folders"));
            enable->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(enable, SIGNAL(triggered(bool)), SLOT(slotPauseAllFolders()));
        }

        if (accountState->isSignedOut()) {
            QAction *signin = menu->addAction(tr("Log in..."));
            signin->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signin, SIGNAL(triggered()), this, SLOT(slotLogin()));
        } else {
            QAction *signout = menu->addAction(tr("Log out"));
            signout->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signout, SIGNAL(triggered()), this, SLOT(slotLogout()));
        }
    }
}

void ownCloudGui::slotContextMenuAboutToShow()
{
    // For some reason on OS X _contextMenu->isVisible returns always false
    _contextMenuVisibleOsx = true;

    // Update icon in sys tray, as it might change depending on the context menu state
    slotComputeOverallSyncStatus();
}

void ownCloudGui::slotContextMenuAboutToHide()
{
    // For some reason on OS X _contextMenu->isVisible returns always false
    _contextMenuVisibleOsx = false;

    // Update icon in sys tray, as it might change depending on the context menu state
    slotComputeOverallSyncStatus();
}

bool ownCloudGui::contextMenuVisible() const
{
#ifdef Q_OS_MAC
    return _contextMenuVisibleOsx;
#else
    return _contextMenu->isVisible();
#endif
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

static QByteArray forceQDBusTrayWorkaround()
{
    static QByteArray var = qgetenv("OWNCLOUD_FORCE_QDBUS_TRAY_WORKAROUND");
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
        _contextMenu->addAction(_actionQuit);
        return;
    }

// Enables workarounds for bugs introduced in Qt 5.5.0
// In particular QTBUG-47863 #3672 (tray menu fails to update and
// becomes unresponsive) and QTBUG-48068 #3722 (click signal is
// emitted several times)
// The Qt version check intentionally uses 5.0.0 (where platformMenu()
// was introduced) instead of 5.5.0 to avoid issues where the Qt
// version used to build is different from the one used at runtime.
// If we build with 5.6.1 or newer, we can skip this because the
// bugs should be fixed there.
#ifdef Q_OS_LINUX
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)) && (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
    if (qVersion() == QByteArray("5.5.0")) {
        QObject *platformMenu = reinterpret_cast<QObject *>(_tray->contextMenu()->platformMenu());
        if (platformMenu
            && platformMenu->metaObject()->className() == QLatin1String("QDBusPlatformMenu")) {
            _qdbusmenuWorkaround = true;
            qCWarning(lcApplication) << "Enabled QDBusPlatformMenu workaround";
        }
    }
#endif
#endif

    if (forceQDBusTrayWorkaround() == "1") {
        _qdbusmenuWorkaround = true;
    } else if (forceQDBusTrayWorkaround() == "0") {
        _qdbusmenuWorkaround = false;
    }

    // When the qdbusmenuWorkaround is necessary, we can't do on-demand updates
    // because the workaround is to hide and show the tray icon.
    if (_qdbusmenuWorkaround) {
        connect(&_workaroundBatchTrayUpdate, SIGNAL(timeout()), SLOT(updateContextMenu()));
        _workaroundBatchTrayUpdate.setInterval(30 * 1000);
        _workaroundBatchTrayUpdate.setSingleShot(true);
    } else {
// Update the context menu whenever we're about to show it
// to the user.
#ifdef Q_OS_MAC
        // https://bugreports.qt.io/browse/QTBUG-54633
        connect(_contextMenu.data(), SIGNAL(aboutToShow()), SLOT(slotContextMenuAboutToShow()));
        connect(_contextMenu.data(), SIGNAL(aboutToHide()), SLOT(slotContextMenuAboutToHide()));
#else
        connect(_contextMenu.data(), SIGNAL(aboutToShow()), SLOT(updateContextMenu()));
#endif
    }

    // Populate the context menu now.
    updateContextMenu();
}

void ownCloudGui::updateContextMenu()
{
    if (minimalTrayMenu()) {
        return;
    }

    if (_qdbusmenuWorkaround) {
        // To make tray menu updates work with these bugs (see setupContextMenu)
        // we need to hide and show the tray icon. We don't want to do that
        // while it's visible!
        if (contextMenuVisible()) {
            if (!_workaroundBatchTrayUpdate.isActive()) {
                _workaroundBatchTrayUpdate.start();
            }
            return;
        }
        _tray->hide();
    }

    _contextMenu->clear();
    slotRebuildRecentMenus();

    // We must call deleteLater because we might be called from the press in one of the actions.
    foreach (auto menu, _accountMenus) {
        menu->deleteLater();
    }
    _accountMenus.clear();


    auto accountList = AccountManager::instance()->accounts();

    bool isConfigured = (!accountList.isEmpty());
    bool atLeastOneConnected = false;
    bool atLeastOneSignedOut = false;
    bool atLeastOneSignedIn = false;
    bool atLeastOnePaused = false;
    bool atLeastOneNotPaused = false;
    foreach (auto a, accountList) {
        if (a->isConnected()) {
            atLeastOneConnected = true;
        }
        if (a->isSignedOut()) {
            atLeastOneSignedOut = true;
        } else {
            atLeastOneSignedIn = true;
        }
    }
    foreach (auto f, FolderMan::instance()->map()) {
        if (f->syncPaused()) {
            atLeastOnePaused = true;
        } else {
            atLeastOneNotPaused = true;
        }
    }

    if (accountList.count() > 1) {
        foreach (AccountStatePtr account, accountList) {
            QMenu *accountMenu = new QMenu(account->account()->displayName(), _contextMenu.data());
            _accountMenus.append(accountMenu);
            _contextMenu->addMenu(accountMenu);

            addAccountContextMenu(account, accountMenu, true);
        }
    } else if (accountList.count() == 1) {
        addAccountContextMenu(accountList.first(), _contextMenu.data(), false);
    }

    _contextMenu->addSeparator();

    if (isConfigured && atLeastOneConnected) {
        _contextMenu->addAction(_actionStatus);
        _contextMenu->addMenu(_recentActionsMenu);
        _contextMenu->addSeparator();
    }
    if (accountList.isEmpty()) {
        _contextMenu->addAction(_actionNewAccountWizard);
    }
    _contextMenu->addAction(_actionSettings);
    if (!Theme::instance()->helpUrl().isEmpty()) {
        _contextMenu->addAction(_actionHelp);
    }

    if (_actionCrash) {
        _contextMenu->addAction(_actionCrash);
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
        connect(action, SIGNAL(triggered(bool)), SLOT(slotUnpauseAllFolders()));
    }
    if (atLeastOneNotPaused) {
        QString text;
        if (accountList.count() > 1) {
            text = tr("Pause all synchronization");
        } else {
            text = tr("Pause synchronization");
        }
        QAction *action = _contextMenu->addAction(text);
        connect(action, SIGNAL(triggered(bool)), SLOT(slotPauseAllFolders()));
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
    _contextMenu->addAction(_actionQuit);

    if (_qdbusmenuWorkaround) {
        _tray->show();
    }
}

void ownCloudGui::updateContextMenuNeeded()
{
    // For the workaround case updating while visible is impossible. Instead
    // occasionally update the menu when it's invisible.
    if (_qdbusmenuWorkaround) {
        if (!_workaroundBatchTrayUpdate.isActive()) {
            _workaroundBatchTrayUpdate.start();
        }
        return;
    }

#ifdef Q_OS_MAC
    // https://bugreports.qt.io/browse/QTBUG-54845
    // We cannot update on demand or while visible -> update when invisible.
    if (!contextMenuVisible()) {
        updateContextMenu();
    }
#else
    if (updateWhileVisible() && contextMenuVisible())
        updateContextMenu();
#endif

    // If no update was done here, we might update it on-demand due to
    // the aboutToShow() signal.
}

void ownCloudGui::slotShowTrayMessage(const QString &title, const QString &msg)
{
    if (_tray)
        _tray->showMessage(title, msg);
    else
        qCWarning(lcApplication) << "Tray not ready: " << msg;
}

void ownCloudGui::slotShowOptionalTrayMessage(const QString &title, const QString &msg)
{
    ConfigFile cfg;
    if (cfg.optionalDesktopNotifications()) {
        slotShowTrayMessage(title, msg);
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

void ownCloudGui::setupActions()
{
    _actionStatus = new QAction(tr("Unknown status"), this);
    _actionStatus->setEnabled(false);
    _actionSettings = new QAction(tr("Settings..."), this);
    _actionNewAccountWizard = new QAction(tr("New account..."), this);
    _actionRecent = new QAction(tr("Details..."), this);
    _actionRecent->setEnabled(true);

    QObject::connect(_actionRecent, SIGNAL(triggered(bool)), SLOT(slotShowSyncProtocol()));
    QObject::connect(_actionSettings, SIGNAL(triggered(bool)), SLOT(slotShowSettings()));
    QObject::connect(_actionNewAccountWizard, SIGNAL(triggered(bool)), SLOT(slotNewAccountWizard()));
    _actionHelp = new QAction(tr("Help"), this);
    QObject::connect(_actionHelp, SIGNAL(triggered(bool)), SLOT(slotHelp()));
    _actionQuit = new QAction(tr("Quit %1").arg(Theme::instance()->appNameGUI()), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), _app, SLOT(quit()));

    _actionLogin = new QAction(tr("Log in..."), this);
    connect(_actionLogin, SIGNAL(triggered()), this, SLOT(slotLogin()));
    _actionLogout = new QAction(tr("Log out"), this);
    connect(_actionLogout, SIGNAL(triggered()), this, SLOT(slotLogout()));

    if (_app->debugMode()) {
        _actionCrash = new QAction(tr("Crash now", "Only shows in debug mode to allow testing the crash handler"), this);
        connect(_actionCrash, SIGNAL(triggered()), _app, SLOT(slotCrash()));
    } else {
        _actionCrash = 0;
    }
}

void ownCloudGui::slotRebuildRecentMenus()
{
    _recentActionsMenu->clear();
    if (!_recentItemsActions.isEmpty()) {
        foreach (QAction *a, _recentItemsActions) {
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


void ownCloudGui::slotUpdateProgress(const QString &folder, const ProgressInfo &progress)
{
    Q_UNUSED(folder);

    if (!progress._currentDiscoveredFolder.isEmpty()) {
        _actionStatus->setText(tr("Checking for changes in '%1'")
                                   .arg(progress._currentDiscoveredFolder));
    } else if (progress.totalSize() == 0) {
        quint64 currentFile = progress.currentFile();
        quint64 totalFileCount = qMax(progress.totalFiles(), currentFile);
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
            QIcon warnIcon(":/client/resources/warning");
            _actionRecent->setIcon(warnIcon);
        }

        QString kindStr = Progress::asResultString(progress._lastCompletedItem);
        QString timeStr = QTime::currentTime().toString("hh:mm");
        QString actionText = tr("%1 (%2, %3)").arg(progress._lastCompletedItem._file, kindStr, timeStr);
        QAction *action = new QAction(actionText, this);
        Folder *f = FolderMan::instance()->folder(folder);
        if (f) {
            QString fullPath = f->path() + '/' + progress._lastCompletedItem._file;
            if (QFile(fullPath).exists()) {
                _recentItemsMapper->setMapping(action, fullPath);
                connect(action, SIGNAL(triggered()), _recentItemsMapper, SLOT(map()));
            } else {
                action->setEnabled(false);
            }
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

    if (progress.isUpdatingEstimates()
        && progress.completedFiles() >= progress.totalFiles()
        && progress._currentDiscoveredFolder.isEmpty()) {
        QTimer::singleShot(2000, this, SLOT(slotDisplayIdle()));
    }
}

void ownCloudGui::slotDisplayIdle()
{
    _actionStatus->setText(tr("Up to date"));
}

void ownCloudGui::slotLogin()
{
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
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

void ownCloudGui::slotUnpauseAllFolders()
{
    setPauseOnAllFoldersHelper(false);
}

void ownCloudGui::slotPauseAllFolders()
{
    setPauseOnAllFoldersHelper(true);
}

void ownCloudGui::slotNewAccountWizard()
{
    OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)));
}

void ownCloudGui::setPauseOnAllFoldersHelper(bool pause)
{
    QList<AccountState *> accounts;
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        accounts.append(account.data());
    } else {
        foreach (auto a, AccountManager::instance()->accounts()) {
            accounts.append(a.data());
        }
    }
    foreach (Folder *f, FolderMan::instance()->map()) {
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
    QMessageBox *msgBox = new QMessageBox;
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
        _settingsDialog =
#if defined(Q_OS_MAC)
            new SettingsDialogMac(this);
#else
            new SettingsDialog(this);
#endif
        _settingsDialog->setAttribute(Qt::WA_DeleteOnClose, true);
        _settingsDialog->show();
    }
    raiseDialog(_settingsDialog.data());
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
        QDesktopServices::openUrl(account->url());
    }
}

void ownCloudGui::slotHelp()
{
    QDesktopServices::openUrl(QUrl(Theme::instance()->helpUrl()));
}

void ownCloudGui::raiseDialog(QWidget *raiseWidget)
{
    if (raiseWidget && raiseWidget->parentWidget() == 0) {
        // Qt has a bug which causes parent-less dialogs to pop-under.
        raiseWidget->showNormal();
        raiseWidget->raise();
        raiseWidget->activateWindow();

#if defined(Q_OS_MAC)
        // viel hilft viel ;-)
        MacWindow::bringToFront(raiseWidget);
#endif
#if defined(Q_OS_X11)
        WId wid = widget->winId();
        NETWM::init();

        XEvent e;
        e.xclient.type = ClientMessage;
        e.xclient.message_type = NETWM::NET_ACTIVE_WINDOW;
        e.xclient.display = QX11Info::display();
        e.xclient.window = wid;
        e.xclient.format = 32;
        e.xclient.data.l[0] = 2;
        e.xclient.data.l[1] = QX11Info::appTime();
        e.xclient.data.l[2] = 0;
        e.xclient.data.l[3] = 0l;
        e.xclient.data.l[4] = 0l;
        Display *display = QX11Info::display();
        XSendEvent(display,
            RootWindow(display, DefaultScreen(display)),
            False, // propagate
            SubstructureRedirectMask | SubstructureNotifyMask,
            &e);
#endif
    }
}


void ownCloudGui::slotShowShareDialog(const QString &sharePath, const QString &localPath, bool resharingAllowed)
{
    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        qCWarning(lcApplication) << "Could not open share dialog for" << localPath << "no responsible folder found";
        return;
    }

    // For https://github.com/owncloud/client/issues/3783
    _settingsDialog->hide();

    const auto accountState = folder->accountState();

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
        maxSharingPermissions = 0;
    }


    ShareDialog *w = 0;
    if (_shareDialogs.contains(localPath) && _shareDialogs[localPath]) {
        qCInfo(lcApplication) << "Raising share dialog" << sharePath << localPath;
        w = _shareDialogs[localPath];
    } else {
        qCInfo(lcApplication) << "Opening share dialog" << sharePath << localPath << maxSharingPermissions;
        w = new ShareDialog(accountState, sharePath, localPath, maxSharingPermissions);
        w->setAttribute(Qt::WA_DeleteOnClose, true);

        _shareDialogs[localPath] = w;
        connect(w, SIGNAL(destroyed(QObject *)), SLOT(slotRemoveDestroyedShareDialogs()));
    }
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


} // end namespace
