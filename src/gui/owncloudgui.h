/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef OWNCLOUDGUI_H
#define OWNCLOUDGUI_H

#include "systray.h"
#include "connectionvalidator.h"
#include "progressdispatcher.h"

#include <QObject>
#include <QPointer>
#include <QAction>
#include <QMenu>
#include <QSize>
#include <QTimer>
#ifdef WITH_LIBCLOUDPROVIDERS
#include <QDBusConnection>
#endif

namespace OCC {

class Folder;

class SettingsDialog;
class ShareDialog;
class Application;
class LogBrowser;
class AccountState;

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

    bool checkAccountExists(bool openSettings);

    static void raiseDialog(QWidget *raiseWidget);
    static QSize settingsDialogSize() { return {720, 500}; }
    void setupOverlayIcons();
#ifdef WITH_LIBCLOUDPROVIDERS
    void setupCloudProviders();
    bool cloudProviderApiAvailable();
#endif
    void createTray();

    void hideAndShowTray();

signals:
    void setupProxy();
    void serverError(int code, const QString &message);
    void isShowingSettingsDialog();

public slots:
    void slotComputeOverallSyncStatus();
    void slotShowTrayMessage(const QString &title, const QString &msg);
    void slotShowTrayUpdateMessage(const QString &title, const QString &msg, const QUrl &webUrl);
    void slotFolderOpenAction(const QString &alias);
    void slotUpdateProgress(const QString &folder, const OCC::ProgressInfo &progress);
    void slotShowGuiMessage(const QString &title, const QString &message);
    void slotShowSettings();
    void slotShowSyncProtocol();
    void slotShutdown();
    void slotSyncStateChange(OCC::Folder *);
    void slotTrayClicked(QSystemTrayIcon::ActivationReason reason);
    void slotToggleLogBrowser();
    void slotOpenOwnCloud();
    void slotOpenSettingsDialog();
    void slotOpenMainDialog();
    void slotSettingsDialogActivated();
    void slotHelp();
    void slotOpenPath(const QString &path);
    void slotTrayMessageIfServerUnsupported(const OCC::AccountPtr &account);
    void slotNeedToAcceptTermsOfService(const OCC::AccountPtr &account,
                                        OCC::AccountState::State state);

    /**
     * Open a share dialog for a file or folder.
     *
     * localPath is the absolute local path to it (so not relative
     * to the folder).
     */
    void slotShowShareDialog(const QString &localPath) const;
    void slotShowFileActivityDialog(const QString &localPath) const;
    void slotShowFileActionsDialog(const QString &localPath) const;
    void slotNewAccountWizard();

private slots:
    void slotLogin();
    void slotLogout();

private:
    QPointer<Systray> _tray;
    QPointer<SettingsDialog> _settingsDialog;
    QPointer<LogBrowser> _logBrowser;

#ifdef WITH_LIBCLOUDPROVIDERS
    QDBusConnection _bus;
#endif

    QAction *_actionNewAccountWizard = nullptr;
    QAction *_actionSettings = nullptr;
    QAction *_actionEstimate = nullptr;


    QList<QAction *> _recentItemsActions;
    Application *_app;
};

} // namespace OCC

#endif // OWNCLOUDGUI_H
