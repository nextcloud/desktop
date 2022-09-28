/*
 * Copyright (C) by Cédric Bellegarde <gnumdk@gmail.com>
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

#ifndef SYSTRAY_H
#define SYSTRAY_H

#include <QSystemTrayIcon>

#include "accountmanager.h"
#include "tray/usermodel.h"

#include <QQmlNetworkAccessManagerFactory>

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
void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *window);
double menuBarThickness();
#endif

/**
 * @brief The Systray class
 * @ingroup gui
 */
class Systray
    : public QSystemTrayIcon
{
    Q_OBJECT

    Q_PROPERTY(QString windowTitle READ windowTitle CONSTANT)
    Q_PROPERTY(bool useNormalWindow READ useNormalWindow CONSTANT)
    Q_PROPERTY(bool syncIsPaused READ syncIsPaused WRITE setSyncIsPaused NOTIFY syncIsPausedChanged)
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)

public:
    static Systray *instance();
    ~Systray() override = default;

    enum class TaskBarPosition { Bottom, Left, Top, Right };
    Q_ENUM(TaskBarPosition);

    enum class NotificationPosition { Default, TopLeft, TopRight, BottomLeft, BottomRight };
    Q_ENUM(NotificationPosition);

    enum class WindowPosition { Default, Center };
    Q_ENUM(WindowPosition);

    void setTrayEngine(QQmlApplicationEngine *trayEngine);
    void create();
    void showMessage(const QString &title, const QString &message, MessageIcon icon = Information);
    void showUpdateMessage(const QString &title, const QString &message, const QUrl &webUrl);
    void setToolTip(const QString &tip);
    void createCallDialog(const Activity &callNotification, const AccountStatePtr accountState);
    void createEditFileLocallyLoadingDialog(const QString &fileName);
    void destroyEditFileLocallyLoadingDialog();

    Q_REQUIRED_RESULT QString windowTitle() const;
    Q_REQUIRED_RESULT bool useNormalWindow() const;

    Q_REQUIRED_RESULT bool syncIsPaused() const;
    Q_REQUIRED_RESULT bool isOpen() const;

signals:
    void currentUserChanged();
    void openAccountWizard();
    void openSettings();
    void openHelp();
    void shutdown();

    void openShareDialog(const QString &sharePath, const QString &localPath);
    void showFileActivityDialog(const QString &objectName, const int objectId);
    void sendChatMessage(const QString &token, const QString &message, const QString &replyTo);
    void showErrorMessageDialog(const QString &error);

    void syncIsPausedChanged();
    void isOpenChanged();

public slots:
    void slotCurrentUserChanged();

    void forceWindowInit(QQuickWindow *window) const;
    void positionWindowAtTray(QQuickWindow *window) const;
    void positionWindowAtScreenCenter(QQuickWindow *window) const;
    void positionNotificationWindow(QQuickWindow *window) const;

    // Do not use this for QQuickWindow components managed by the QML engine,
    // only for those managed by the C++ engine
    void destroyDialog(QQuickWindow *window) const;

    void showWindow(WindowPosition position = WindowPosition::Default);
    void hideWindow();

    void setSyncIsPaused(const bool syncIsPaused);
    void setIsOpen(const bool isOpen);

private slots:
    void slotUnpauseAllFolders();
    void slotPauseAllFolders();

private:
    void setPauseOnAllFoldersHelper(bool pause);

    static Systray *_instance;
    Systray();

    void setupContextMenu();

    QScreen *currentScreen() const;
    QRect currentScreenRect() const;
    QPoint computeWindowReferencePoint() const;
    QPoint computeNotificationReferencePoint(int spacing = 20, NotificationPosition position = NotificationPosition::Default) const;
    QPoint calcTrayIconCenter() const;
    TaskBarPosition taskbarOrientation() const;
    QRect taskbarGeometry() const;
    QRect computeWindowRect(int spacing, const QPoint &topLeft, const QPoint &bottomRight) const;
    QPoint computeWindowPosition(int width, int height) const;
    QPoint computeNotificationPosition(int width, int height, int spacing = 20, NotificationPosition position = NotificationPosition::Default) const;

    bool _isOpen = false;
    bool _syncIsPaused = true;
    QPointer<QQmlApplicationEngine> _trayEngine;
    QPointer<QMenu> _contextMenu;
    QSharedPointer<QQuickWindow> _trayWindow;

    AccessManagerFactory _accessManagerFactory;

    QSet<qlonglong> _callsAlreadyNotified;

    QPointer<QObject> _editFileLocallyLoadingDialog;
};

} // namespace OCC

#endif //SYSTRAY_H
