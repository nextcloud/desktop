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
#ifdef Q_OS_MACOS
    void slotShowSettingsForSandboxReapproval();
#endif
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
    void slotShowGovernanceLabelsDialog(AccountPtr account,
                                        const QString &localPath,
                                        const QString &fileId) const;
    void slotShowFileActivityDialog(const QString &localPath) const;
    void slotShowFileActionsDialog(const QString &localPath) const;
#ifdef BUILD_FILE_PROVIDER_MODULE
    /**
     * @brief Open an item's web page in the user's browser on behalf of the macOS file provider extension.
     *
     * Resolves the per-item private link via `fetchPrivateLinkUrl` (PROPFIND for
     * the server-side `privatelink` property, falling back to the deprecated link
     * built from the numeric file id) and opens it via `Utility::openBrowser`.
     * Mirrors the classic-sync entry exposed by `SocketApi::command_OPEN_PRIVATE_LINK`.
     * See nextcloud/desktop#10025.
     *
     * @param fileId The numeric server file id (WebDAV `fileid`). Not the ocId.
     * @param remoteItemPath The server-side path of the item.
     * @param fileProviderDomainIdentifier The file provider domain identifier for the account that owns the item.
     */
    void slotOpenItemInBrowserFromFileProvider(const QString &fileId, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);

    /**
     * @brief Copy an item's Nextcloud internal link to the clipboard on behalf of the macOS file provider extension.
     *
     * Resolves the per-item private link via `fetchPrivateLinkUrl` (PROPFIND for
     * the server-side `privatelink` property, falling back to the deprecated link
     * built from the numeric file id) and writes it to `QGuiApplication::clipboard()`.
     * Surfaces a notification through the existing systray afterwards. Mirrors
     * the classic-sync entry exposed by `SocketApi::command_COPY_PRIVATE_LINK`.
     * See nextcloud/desktop#10024.
     *
     * @param fileId The numeric server file id (WebDAV `fileid`). Not the ocId.
     * @param remoteItemPath The server-side path of the item.
     * @param fileProviderDomainIdentifier The file provider domain identifier for the account that owns the item.
     */
    void slotCopyInternalLinkFromFileProvider(const QString &fileId, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);
#endif
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
