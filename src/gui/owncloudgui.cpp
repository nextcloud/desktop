/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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
#    include "settingsdialogmac.h"
#    include "macwindow.h" // qtmacgoodies
#else
#    include "settingsdialog.h"
#endif
#include "logger.h"
#include "logbrowser.h"
#include "account.h"
#include "accountstate.h"
#include "openfilemanager.h"
#include "accountmanager.h"
#include "creds/abstractcredentials.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QSignalMapper>

#if defined(Q_OS_X11)
#include <QX11Info>
#endif

namespace OCC {

const char propertyAccountC[] = "oc_account";

ownCloudGui::ownCloudGui(Application *parent) :
    QObject(parent),
    _tray(0),
#if defined(Q_OS_MAC)
    _settingsDialog(new SettingsDialogMac(this)),
#else
    _settingsDialog(new SettingsDialog(this)),
#endif
    _logBrowser(0),
    _recentActionsMenu(0),
    _folderOpenActionMapper(new QSignalMapper(this)),
    _recentItemsMapper(new QSignalMapper(this)),
    _app(parent)
{
    _tray = new Systray();
    _tray->setParent(this);

    // for the beginning, set the offline icon until the account was verified
    _tray->setIcon( Theme::instance()->folderOfflineIcon(true));

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
    connect( pd, SIGNAL(progressInfo(QString,ProgressInfo)), this,
             SLOT(slotUpdateProgress(QString,ProgressInfo)) );

    FolderMan *folderMan = FolderMan::instance();
    connect( folderMan, SIGNAL(folderSyncStateChange(Folder*)),
             this,SLOT(slotSyncStateChange(Folder*)));

    connect( AccountManager::instance(), SIGNAL(accountAdded(AccountState*)),
             SLOT(setupContextMenu()));
    connect( AccountManager::instance(), SIGNAL(accountRemoved(AccountState*)),
             SLOT(setupContextMenu()));

    connect( Logger::instance(), SIGNAL(guiLog(QString,QString)),
             SLOT(slotShowTrayMessage(QString,QString)));
    connect( Logger::instance(), SIGNAL(optionalGuiLog(QString,QString)),
             SLOT(slotShowOptionalTrayMessage(QString,QString)));
    connect( Logger::instance(), SIGNAL(guiMessage(QString,QString)),
             SLOT(slotShowGuiMessage(QString,QString)));

    setupOverlayIcons();
}

// Use this to do platform specific code to make overlay icons appear
// in the gui
// MacOSX: perform a AppleScript code peace to load the Finder Plugin.


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
              p.waitForReadyRead(-1);
              QByteArray result = p.readAll();
              QString resultAsString(result); // if appropriate
              qDebug() << "Laod Finder Overlay-Plugin: " << resultAsString << ": " << p.exitCode()
                       << (p.exitCode() != 0 ? p.errorString() : QString::null);
        } else  {
            qDebug() << finderExtension << "does not exist! Finder Overlay Plugin loading failed";
        }
    }
#endif
}

// This should rather be in application.... or rather in ConfigFile?
void ownCloudGui::slotOpenSettingsDialog( bool openSettings )
{
    // if account is set up, start the configuration wizard.
    if( !AccountManager::instance()->accounts().isEmpty() ) {
        if( openSettings ) {
            if (_settingsDialog.isNull() || !_settingsDialog->isVisible()) {
                slotShowSettings();
            } else {
                _settingsDialog->close();
            }
        }
    } else {
        qDebug() << "No configured folders yet, starting setup wizard";
        OwncloudSetupWizard::runWizard(qApp, SLOT(slotownCloudWizardDone(int)));
    }
}

void ownCloudGui::slotTrayClicked( QSystemTrayIcon::ActivationReason reason )
{
    // A click on the tray icon should only open the status window on Win and
    // Linux, not on Mac. They want a menu entry.
#if !defined Q_OS_MAC
    if( reason == QSystemTrayIcon::Trigger ) {
        slotOpenSettingsDialog(true); // start settings if config is existing.
    }
#else
    // On Mac, if the settings dialog is already visible but hidden
    // by other applications, this will bring it to the front.
    if( reason == QSystemTrayIcon::Trigger ) {
        if (!_settingsDialog.isNull() && _settingsDialog->isVisible()) {
            slotShowSettings();
        }
    }
#endif
}

void ownCloudGui::slotSyncStateChange( Folder* folder )
{
    slotComputeOverallSyncStatus();

    if( !folder ) {
        return; // Valid, just a general GUI redraw was needed.
    }

    auto result = folder->syncResult();

    qDebug() << "Sync state changed for folder " << folder->alias() << ": "  << result.statusString();

    if (result.status() == SyncResult::Success || result.status() == SyncResult::Error) {
        Logger::instance()->enterNextLogFile();
    }
}

void ownCloudGui::slotFoldersChanged()
{
    slotComputeOverallSyncStatus();
    setupContextMenu();
}

void ownCloudGui::slotOpenPath(const QString &path)
{
    showInFileManager(path);
}

void ownCloudGui::slotAccountStateChanged()
{
    setupContextMenu();
    slotComputeOverallSyncStatus();
}

void ownCloudGui::slotComputeOverallSyncStatus()
{
    bool allSignedOut = true;
    QVector<AccountStatePtr> problemAccounts;
    foreach (auto a, AccountManager::instance()->accounts()) {
        if (!a->isSignedOut()) {
            allSignedOut = false;
        }
        if (!a->isConnected()) {
            problemAccounts.append(a);
        }
    }

    if (!problemAccounts.empty()) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true));
        QStringList messages;
        messages.append(tr("Disconnected from accounts:"));
        foreach (AccountStatePtr a, problemAccounts) {
            QString message = tr("Account %1: %2").arg(
                    a->account()->displayName(), a->stateString(a->state()));
            if (! a->connectionErrors().empty()) {
                message += QLatin1String("\n");
                message += a->connectionErrors().join(QLatin1String("\n"));
            }
            messages.append(message);
        }
        _tray->setToolTip(messages.join(QLatin1String("\n\n")));
        return;
    }

    if (allSignedOut) {
        _tray->setIcon(Theme::instance()->folderOfflineIcon(true));
        _tray->setToolTip(tr("Please sign in"));
        return;
    }

    // display the info of the least successful sync (eg. not just display the result of the latest sync
    QString trayMessage;
    FolderMan *folderMan = FolderMan::instance();
    Folder::Map map = folderMan->map();
    SyncResult overallResult = FolderMan::accountStatus(map.values());

    // create the tray blob message, check if we have an defined state
    if( overallResult.status() != SyncResult::Undefined ) {
        QStringList allStatusStrings;
        if( map.count() > 0 ) {
            foreach(Folder* folder, map.values()) {
                qDebug() << "Folder in overallStatus Message: " << folder << " with name " << folder->alias();
                QString folderMessage = folderMan->statusToString(folder->syncResult().status(), folder->syncPaused());
                allStatusStrings += tr("Folder %1: %2").arg(folder->alias(), folderMessage);
            }

            trayMessage = allStatusStrings.join(QLatin1String("\n"));
        } else {
            trayMessage = tr("No sync folders configured.");
        }

        QIcon statusIcon = Theme::instance()->syncStateIcon( overallResult.status(), true);
        _tray->setIcon( statusIcon );
        _tray->setToolTip(trayMessage);
    } else {
        // undefined because there are no folders.
        QIcon icon = Theme::instance()->syncStateIcon(SyncResult::Problem, true);
        _tray->setIcon( icon );
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

    if (separateMenu) {
        if (accountState->isConnected()) {
            QAction* signout = menu->addAction(tr("Sign out"));
            signout->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signout, SIGNAL(triggered()), this, SLOT(slotLogout()));
        } else {
            QAction* signin = menu->addAction(tr("Sign in..."));
            signin->setProperty(propertyAccountC, QVariant::fromValue(accountState));
            connect(signin, SIGNAL(triggered()), this, SLOT(slotLogin()));
        }
    }

    FolderMan *folderMan = FolderMan::instance();
    bool firstFolder = true;
    bool singleSyncFolder = folderMan->map().size() == 1 && Theme::instance()->singleSyncFolder();
    foreach (Folder* folder, folderMan->map()) {
        if (folder->accountState() != accountState) {
            continue;
        }

        if (firstFolder && !singleSyncFolder) {
            firstFolder = false;
            menu->addSeparator();
            menu->addAction(tr("Managed Folders:"))->setDisabled(true);
        }

        // If there can only be a single sync folder, showing the alias is
        // unnecessary.
        QString folderName = folder->alias();
        if (singleSyncFolder) {
            folderName = Theme::instance()->appNameGUI();
        }

        QAction *action = new QAction( tr("Open folder '%1'").arg(folder->alias()), this );
        connect(action, SIGNAL(triggered()),_folderOpenActionMapper, SLOT(map()));
        _folderOpenActionMapper->setMapping( action, folder->alias() );
        menu->addAction(action);
    }
}

void ownCloudGui::setupContextMenu()
{
    auto accountList = AccountManager::instance()->accounts();

    bool isConfigured = (!accountList.isEmpty());
    bool atLeastOneConnected = false;
    bool atLeastOneDisconnected = false;
    foreach (auto a, accountList) {
        if (a->isConnected()) {
            atLeastOneConnected = true;
        } else {
            atLeastOneDisconnected = true;
        }
    }


    if ( _contextMenu ) {
        _contextMenu->clear();
        _recentActionsMenu->clear();
        _recentActionsMenu->addAction(tr("None."));
        _recentActionsMenu->addAction(_actionRecent);
    } else {
        _contextMenu.reset(new QMenu());
        _recentActionsMenu = new QMenu(tr("Recent Changes"), _contextMenu.data());
        // this must be called only once after creating the context menu, or
        // it will trigger a bug in Ubuntu's SNI bridge patch (11.10, 12.04).
        _tray->setContextMenu(_contextMenu.data());
    }
    _contextMenu->setTitle(Theme::instance()->appNameGUI() );
    // We must call deleteLater because we might be called from the press in one of the action.
    foreach (auto menu, _accountMenus) { menu->deleteLater(); }
    _accountMenus.clear();
    if (accountList.count() > 1) {
        foreach (AccountStatePtr account, accountList) {
            QMenu* accountMenu = new QMenu(account->account()->displayName(), _contextMenu.data());
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
    _contextMenu->addAction(_actionSettings);
    if (!Theme::instance()->helpUrl().isEmpty()) {
        _contextMenu->addAction(_actionHelp);
    }

    if(_actionCrash) {
        _contextMenu->addAction(_actionCrash);
    }

    _contextMenu->addSeparator();
    if (atLeastOneConnected) {
        if (accountList.count() > 1) {
            _actionLogout->setText(tr("Sign out everywhere"));
        } else {
            _actionLogout->setText(tr("Sign out"));
        }
        _contextMenu->addAction(_actionLogout);
    }
    if (atLeastOneDisconnected) {
        if (accountList.count() > 1) {
            _actionLogin->setText(tr("Sign in everywhere..."));
        } else {
            _actionLogin->setText(tr("Sign in..."));
        }
        _contextMenu->addAction(_actionLogin);
    }
    _contextMenu->addAction(_actionQuit);
}


void ownCloudGui::slotShowTrayMessage(const QString &title, const QString &msg)
{
    if( _tray )
        _tray->showMessage(title, msg);
    else
        qDebug() << "Tray not ready: " << msg;
}

void ownCloudGui::slotShowOptionalTrayMessage(const QString &title, const QString &msg)
{
    ConfigFile cfg;
    if (cfg.optionalDesktopNotifications()) {
        slotShowTrayMessage(title, msg);
    }
}


/*
 * open the folder with the given Alais
 */
void ownCloudGui::slotFolderOpenAction( const QString& alias )
{
    Folder *f = FolderMan::instance()->folder(alias);
    if( f ) {
        qDebug() << "opening local url " << f->path();
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
    _actionStatus->setEnabled( false );
    _actionSettings = new QAction(tr("Settings..."), this);
    _actionRecent = new QAction(tr("Details..."), this);
    _actionRecent->setEnabled( true );

    QObject::connect(_actionRecent, SIGNAL(triggered(bool)), SLOT(slotShowSyncProtocol()));
    QObject::connect(_actionSettings, SIGNAL(triggered(bool)), SLOT(slotShowSettings()));
    _actionHelp = new QAction(tr("Help"), this);
    QObject::connect(_actionHelp, SIGNAL(triggered(bool)), SLOT(slotHelp()));
    _actionQuit = new QAction(tr("Quit %1").arg(Theme::instance()->appNameGUI()), this);
    QObject::connect(_actionQuit, SIGNAL(triggered(bool)), _app, SLOT(quit()));

    _actionLogin = new QAction(tr("Sign in..."), this);
    connect(_actionLogin, SIGNAL(triggered()), this, SLOT(slotLogin()));
    _actionLogout = new QAction(tr("Sign out"), this);
    connect(_actionLogout, SIGNAL(triggered()), this, SLOT(slotLogout()));

    if(_app->debugMode()) {
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
        foreach(QAction *a, _recentItemsActions) {
            _recentActionsMenu->addAction(a);
        }
        _recentActionsMenu->addSeparator();
    } else {
        _recentActionsMenu->addAction(tr("No items synced recently"))->setEnabled(false);
    }
    // add a more... entry.
    _recentActionsMenu->addAction(_actionRecent);
}


void ownCloudGui::slotUpdateProgress(const QString &folder, const ProgressInfo& progress)
{
    Q_UNUSED(folder);

    if (!progress._currentDiscoveredFolder.isEmpty()) {
        _actionStatus->setText( tr("Discovering '%1'")
                                .arg( progress._currentDiscoveredFolder ));
    } else if (progress.totalSize() == 0 ) {
        quint64 currentFile = progress.currentFile();
        quint64 totalFileCount = qMax(progress.totalFiles(), currentFile);
        _actionStatus->setText( tr("Syncing %1 of %2  (%3 left)")
            .arg( currentFile ).arg( totalFileCount )
            .arg( Utility::durationToDescriptiveString(progress.totalProgress().estimatedEta) ) );
    } else {
        QString totalSizeStr = Utility::octetsToString( progress.totalSize() );
        _actionStatus->setText( tr("Syncing %1 (%2 left)")
            .arg( totalSizeStr, Utility::durationToDescriptiveString(progress.totalProgress().estimatedEta) ) );
    }




    _actionRecent->setIcon( QIcon() ); // Fixme: Set a "in-progress"-item eventually.

    if (!progress._lastCompletedItem.isEmpty() && !Progress::isIgnoredKind(progress._lastCompletedItem._status)) {

        if (Progress::isWarningKind(progress._lastCompletedItem._status)) {
            // display a warn icon if warnings happend.
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

        slotRebuildRecentMenus();
    }

    if (progress.hasStarted()
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
    auto list = AccountManager::instance()->accounts();
    if (auto account = qvariant_cast<AccountStatePtr>(sender()->property(propertyAccountC))) {
        account->setSignedOut(false);
    } else {
        foreach (const auto &a, list) {
            a->setSignedOut(false);
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
        AccountPtr a = ai->account();
        // invalidate & forget token/password
        a->credentials()->invalidateToken();
        // terminate all syncs and unload folders
        FolderMan *folderMan = FolderMan::instance();
        folderMan->terminateSyncProcess();
        ai->setSignedOut(true);
        // show result
        slotComputeOverallSyncStatus();
    }
}

void ownCloudGui::slotShowGuiMessage(const QString &title, const QString &message)
{
    QMessageBox *msgBox = new QMessageBox;
    msgBox->setAttribute(Qt::WA_DeleteOnClose);
    msgBox->setText(message);
    msgBox->setWindowTitle(title);
    msgBox->setIcon(QMessageBox::Information);
    msgBox->open();
}

void ownCloudGui::slotShowSettings()
{
    qDebug() << Q_FUNC_INFO;
    if (_settingsDialog.isNull()) {
        _settingsDialog =
#if defined(Q_OS_MAC)
                new SettingsDialogMac(this);
#else
                new SettingsDialog(this);
#endif
        _settingsDialog->setAttribute( Qt::WA_DeleteOnClose, true );
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
    if (!_settingsDialog.isNull()) _settingsDialog->close();
    if (!_logBrowser.isNull())     _logBrowser->deleteLater();
}

void ownCloudGui::slotToggleLogBrowser()
{
    if (_logBrowser.isNull()) {
        // init the log browser.
        _logBrowser = new LogBrowser;
        // ## TODO: allow new log name maybe?
    }

    if (_logBrowser->isVisible() ) {
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

void ownCloudGui::raiseDialog( QWidget *raiseWidget )
{
    if( raiseWidget && raiseWidget->parentWidget() == 0) {
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
                   SubstructureRedirectMask|SubstructureNotifyMask,
                   &e);
#endif
    }
}


void ownCloudGui::slotShowShareDialog(const QString &sharePath, const QString &localPath, bool resharingAllowed)
{
    const auto folder = FolderMan::instance()->folderForPath(localPath);
    if (!folder) {
        qDebug() << "Could not open share dialog for" << localPath << "no responsible folder found";
        return;
    }
    const auto accountState = folder->accountState();

    qDebug() << Q_FUNC_INFO << "Opening share dialog" << sharePath << localPath;
    ShareDialog *w = new ShareDialog(accountState->account(), sharePath, localPath, resharingAllowed);
    w->getShares();
    w->setAttribute( Qt::WA_DeleteOnClose, true );
    raiseDialog(w);
}


} // end namespace
