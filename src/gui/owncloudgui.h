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

#ifndef OWNCLOUDGUI_H
#define OWNCLOUDGUI_H

#include "connectionvalidator.h"
#include "progressdispatcher.h"
#include "systray.h"

#include <QObject>
#include <QPointer>
#include <QAction>
#include <QMenu>
#include <QSize>
#include <QTimer>

namespace OCC {

namespace Wizard {
    class SetupWizardController;
}
class Folder;

class AboutDialog;
class SettingsDialog;
class ShareDialog;
class Application;
class LogBrowser;
class AccountState;
class OwncloudSetupWizard;

enum class ShareDialogStartPage {
    UsersAndGroups,
    PublicLinks,
};

/**
 * @brief The ownCloudGui class
 * @ingroup gui
 */
class ownCloudGui : public QObject
{
    Q_OBJECT
public:
    explicit ownCloudGui(Application *parent = nullptr);
    ~ownCloudGui() override;

    bool checkAccountExists(bool openSettings);

    /**
     * Raises our main Window to the front with the raiseWidget in focus.
     * If raiseWidget is a dialog and not visible yet, ->open will be called.
     * For normal widgets we call showNormal.
     */
    static void raiseDialog(QWidget *raiseWidget);
    void setupOverlayIcons();

    /// Whether the tray menu is visible
    bool contextMenuVisible() const;

    void hideAndShowTray();

    SettingsDialog *settingsDialog() const;

    void runNewAccountWizard();

signals:
    void setupProxy();

public slots:
    void setupContextMenu();
    void updateContextMenu();
    void updateContextMenuNeeded();
    void slotContextMenuAboutToShow();
    void slotContextMenuAboutToHide();
    void slotComputeOverallSyncStatus();
    void slotShowTrayMessage(const QString &title, const QString &msg, const QIcon &icon = {});
    void slotShowOptionalTrayMessage(const QString &title, const QString &msg, const QIcon &icon = {});
    void slotFolderOpenAction(Folder *f);
    void slotRebuildRecentMenus();
    void slotUpdateProgress(Folder *folder, const ProgressInfo &progress);
    void slotShowGuiMessage(const QString &title, const QString &message);
    void slotFoldersChanged();
    void slotShowSettings();
    void slotShowSyncProtocol();
    void slotShutdown();
    void slotSyncStateChange(Folder *);
    void slotTrayClicked(QSystemTrayIcon::ActivationReason reason);
    void slotToggleLogBrowser();
    void slotOpenOwnCloud();
    void slotOpenSettingsDialog();
    void slotHelp();
    void slotAbout();
    void slotOpenPath(const QString &path);
    void slotAccountStateChanged();
    void slotTrayMessageIfServerUnsupported(Account *account);

    /**
     * Open a share dialog for a file or folder.
     *
     * sharePath is the full remote path to the item,
     * localPath is the absolute local path to it (so not relative
     * to the folder).
     */
    void slotShowShareDialog(const QString &sharePath, const QString &localPath, ShareDialogStartPage startPage);

    void slotRemoveDestroyedShareDialogs();

private slots:
    void slotLogin();
    void slotLogout();
    void slotUnpauseAllFolders();
    void slotPauseAllFolders();

private:
    void setPauseOnAllFoldersHelper(bool pause);
    void setupActions();
    void addAccountContextMenu(AccountStatePtr accountState, QMenu *menu, bool separateMenu);

    Systray *_tray;
    SettingsDialog *_settingsDialog;
    // tray's menu
    QScopedPointer<QMenu> _contextMenu;

    // Manually tracking whether the context menu is visible via aboutToShow
    // and aboutToHide. Unfortunately aboutToHide isn't reliable everywhere
    // so this only gets used with _workaroundManualVisibility (when the tray's
    // isVisible() is unreliable)
    bool _contextMenuVisibleManual = false;

    QMenu *_recentActionsMenu;
    QVector<QMenu *> _accountMenus;
    bool _workaroundShowAndHideTray = false;
    bool _workaroundNoAboutToShowUpdate = false;
    bool _workaroundFakeDoubleClick = false;
    bool _workaroundManualVisibility = false;
    QTimer _delayedTrayUpdateTimer;
    QMap<QString, QPointer<ShareDialog>> _shareDialogs;

    QAction *_actionLogin;
    QAction *_actionLogout;

    QAction *_actionNewAccountWizard;
    QAction *_actionSettings;
    QAction *_actionStatus;
    QAction *_actionEstimate;
    QAction *_actionRecent;
    QAction *_actionHelp;
    QAction *_actionAbout;
    QAction *_actionQuit;
    QAction *_actionCrash;
    QAction *_actionCrashEnforce;
    QAction *_actionCrashFatal;


    QList<QAction *> _recentItemsActions;
    Application *_app;

    // keeping a pointer on those dialogs allows us to make sure they will be shown only once
    QPointer<Wizard::SetupWizardController> _wizardController;
    QPointer<AboutDialog> _aboutDialog;
};

} // namespace OCC

#endif // OWNCLOUDGUI_H
