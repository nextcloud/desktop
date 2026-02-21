/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SYSTRAY_H
#define SYSTRAY_H

#include "accountmanager.h"
#include "tray/usermodel.h"

#include <QSystemTrayIcon>
#include <QQmlNetworkAccessManagerFactory>
#include <QStringListModel>

class QScreen;
class QQmlApplicationEngine;
class QQuickWindow;
class QWindow;
class QQuickWindow;
class QGuiApplication;

namespace OCC {

class AccessManagerFactory : public QQmlNetworkAccessManagerFactory
{
public:
    AccessManagerFactory();

    QNetworkAccessManager* create(QObject *parent) override;
};

#ifdef Q_OS_MACOS
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_14
enum MacNotificationAuthorizationOptions {
    Default = 0,
    Provisional
};

void setUserNotificationCenterDelegate();
void checkNotificationAuth(MacNotificationAuthorizationOptions authOptions = MacNotificationAuthorizationOptions::Provisional);
void registerNotificationCategories(const QString &localizedDownloadString);
bool canOsXSendUserNotification();
void sendOsXUserNotification(const QString &title, const QString &message);
void sendOsXUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl);
void sendOsXTalkNotification(const QString &title, const QString &message, const QString &token, const QString &replyTo, const AccountStatePtr accountState);
#endif
void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *window);
double menuBarThickness();
#endif

/**
 * @brief The Systray class
 * @ingroup gui
 */
class Systray : public QSystemTrayIcon
{
    Q_OBJECT

    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
    Q_PROPERTY(bool useNormalWindow READ useNormalWindow CONSTANT)
    Q_PROPERTY(bool syncIsPaused READ syncIsPaused WRITE setSyncIsPaused NOTIFY syncIsPausedChanged)
    Q_PROPERTY(bool anySyncFolders READ anySyncFolders NOTIFY anySyncFoldersChanged)
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    Q_PROPERTY(bool enableAddAccount READ enableAddAccount CONSTANT)

public:
    static Systray *instance();
    ~Systray() override = default;

    enum class TaskBarPosition { Bottom, Left, Top, Right };
    Q_ENUM(TaskBarPosition);

    enum class NotificationPosition { Default, TopLeft, TopRight, BottomLeft, BottomRight };
    Q_ENUM(NotificationPosition);

    enum class WindowPosition { Default, Center };
    Q_ENUM(WindowPosition);

    enum class FileDetailsPage { Activity, Sharing };
    Q_ENUM(FileDetailsPage);

    [[nodiscard]] QString windowTitle() const;
    [[nodiscard]] bool useNormalWindow() const;

    [[nodiscard]] bool syncIsPaused() const;
    [[nodiscard]] bool anySyncFolders() const;
    [[nodiscard]] bool isOpen() const;

    [[nodiscard]] bool enableAddAccount() const;

    bool raiseDialogs();

    [[nodiscard]] QQmlApplicationEngine* trayEngine() const;

signals:
    void currentUserChanged();
    void openAccountWizard();
    void openSettings();
    void openHelp();
    void shutdown();

    void showFileDetailsPage(const QString &fileLocalPath, const OCC::Systray::FileDetailsPage page);
    void showFileDetails(OCC::AccountState *accountState, const QString &localPath, const OCC::Systray::FileDetailsPage fileDetailsPage);
    void sendChatMessage(const QString &token, const QString &message, const QString &replyTo);
    void showErrorMessageDialog(const QString &error);

    void syncIsPausedChanged();
    void anySyncFoldersChanged();
    void isOpenChanged();

    void hideSettingsDialog();

public slots:
    void setTrayEngine(QQmlApplicationEngine *trayEngine);
    void create();

    void showMessage(const QString &title, const QString &message, QSystemTrayIcon::MessageIcon icon = Information);
    void showUpdateMessage(const QString &title, const QString &message, const QUrl &webUrl);
    void showTalkMessage(const QString &title, const QString &message, const QString &replyTo, const QString &token, const OCC::AccountStatePtr &accountState);

    void createCallDialog(const OCC::Activity &callNotification, const OCC::AccountStatePtr accountState);
    void createEditFileLocallyLoadingDialog(const QString &fileName);
    void destroyEditFileLocallyLoadingDialog();
    void createResolveConflictsDialog(const OCC::ActivityList &allConflicts);
    void createEncryptionTokenDiscoveryDialog();
    void destroyEncryptionTokenDiscoveryDialog();

    void slotCurrentUserChanged();

    void forceWindowInit(QQuickWindow *window) const;
    void positionWindowAtTray(QQuickWindow *window) const;
    void positionWindowAtScreenCenter(QQuickWindow *window) const;
    void positionNotificationWindow(QQuickWindow *window) const;

    // Do not use this for QQuickWindow components managed by the QML engine,
    // only for those managed by the C++ engine
    void destroyDialog(QQuickWindow *window) const;

    void showWindow(OCC::Systray::WindowPosition position = OCC::Systray::WindowPosition::Default);
    void hideWindow();

    void setSyncIsPaused(const bool syncIsPaused);
    void setIsOpen(const bool isOpen);

    void createShareDialog(const QString &localPath);
    void createFileActivityDialog(const QString &localPath);
    void showFileActionsDialog(const QString &localPath);

    #ifdef BUILD_FILE_PROVIDER_MODULE

    void slotShowFileProviderFileActionsDialog(const QString &fileId, const QString &localPath, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);

    #endif

    void presentShareViewInTray(const QString &localPath);
    void presentFileActionsViewInSystray(const QString &localPath);

private slots:
    void slotUpdateSyncPausedState();
    void slotUnpauseAllFolders();
    void slotPauseAllFolders();
    void slotSyncFoldersChanged(const OCC::Folder::Map &foldeMap);

private:
    // Argument allows user to specify a specific dialog to be raised
    bool raiseFileDetailDialogs(const QString &localPath = {});
    void setPauseOnAllFoldersHelper(bool pause);

    static Systray *_instance;
    Systray();

    void setupContextMenu();
    void createFileDetailsDialog(const QString &localPath);
    void createFileActionsDialog(const QString &localPath);

    #ifdef BUILD_FILE_PROVIDER_MODULE

    void createFileProviderFileActionsDialog(const QString &fileId, const QString &localPath, const QString &remoteItemPath, const QString &fileProviderDomainIdentifier);

    #endif

    /**
     * @param fileId This is optional and null by default. Only when there are no synchronization folders configured (for example when only a file provider is used) is this value required.
     * @param remoteItemPath The server-side path of the item. Used as a fallback to determine the success response URL when no sync folder is configured.
     */
    void createFileActionsDialogWithAccountState(const QString &localPath, AccountState *accountState, const QString &fileId = {}, const QString &remoteItemPath = {});

    [[nodiscard]] QScreen *currentScreen() const;
    [[nodiscard]] QRect currentScreenRect() const;
    [[nodiscard]] QRect currentAvailableScreenRect() const;
    [[nodiscard]] QPoint computeWindowReferencePoint() const;
    [[nodiscard]] QPoint computeNotificationReferencePoint(int spacing = 20, NotificationPosition position = NotificationPosition::Default) const;
    [[nodiscard]] QPoint calcTrayIconCenter() const;
    [[nodiscard]] TaskBarPosition taskbarOrientation() const;
    [[nodiscard]] QRect computeWindowRect(int spacing, const QPoint &topLeft, const QPoint &bottomRight) const;
    [[nodiscard]] QPoint computeWindowPosition(int width, int height) const;
    [[nodiscard]] QPoint computeNotificationPosition(int width, int height, int spacing = 20, NotificationPosition position = NotificationPosition::Default) const;

    bool _isOpen = false;
    bool _syncIsPaused = true;
    bool _anySyncFolders = false;

    std::unique_ptr<QQmlApplicationEngine> _trayEngine;
    QPointer<QMenu> _contextMenu;
    QSharedPointer<QQuickWindow> _trayWindow;

    AccessManagerFactory _accessManagerFactory;

    QSet<qint64> _callsAlreadyNotified;
    QPointer<QObject> _editFileLocallyLoadingDialog;
    QPointer<QObject> _encryptionTokenDiscoveryDialog;
    QVector<QQuickWindow*> _fileDetailDialogs;

    QStringListModel _fakeActivityModel;
};

} // namespace OCC

#endif //SYSTRAY_H
