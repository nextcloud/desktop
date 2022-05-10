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
void setUserNotificationCenterDelegate();
void checkNotificationAuth();
void registerNotificationCategories(const QString &localizedDownloadString);
bool canOsXSendUserNotification();
void sendOsXUserNotification(const QString &title, const QString &message);
void sendOsXUpdateNotification(const QString &title, const QString &message, const QUrl &webUrl);
void setTrayWindowLevelAndVisibleOnAllSpaces(QWindow *window);
double statusBarThickness();
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

public:
    static Systray *instance();
    ~Systray() override = default;

    enum class TaskBarPosition { Bottom, Left, Top, Right };
    Q_ENUM(TaskBarPosition);
    
    enum class NotificationPosition { Default, TopLeft, TopRight, BottomLeft, BottomRight };
    Q_ENUM(NotificationPosition);

    void setTrayEngine(QQmlApplicationEngine *trayEngine);
    void create();
    void showMessage(const QString &title, const QString &message, MessageIcon icon = Information);
    void showUpdateMessage(const QString &title, const QString &message, const QUrl &webUrl);
    void setToolTip(const QString &tip);
    bool isOpen();
    QString windowTitle() const;
    bool useNormalWindow() const;
    void createCallDialog(const Activity &callNotification);

    Q_INVOKABLE void pauseResumeSync();
    Q_INVOKABLE bool syncIsPaused();
    Q_INVOKABLE void setOpened();
    Q_INVOKABLE void setClosed();
    Q_INVOKABLE void positionWindow(QQuickWindow *window) const;
    Q_INVOKABLE void forceWindowInit(QQuickWindow *window) const;
    Q_INVOKABLE void positionNotificationWindow(QQuickWindow *window) const;

signals:
    void currentUserChanged();
    void openAccountWizard();
    void openMainDialog();
    void openSettings();
    void openHelp();
    void shutdown();

    void hideWindow();
    void showWindow();
    void openShareDialog(const QString &sharePath, const QString &localPath);
    void showFileActivityDialog(const QString &objectName, const int objectId);
    void sendChatMessage(const QString &token, const QString &message, const QString &replyTo);
    void showErrorMessageDialog(const QString &error);

public slots:
    void slotNewUserSelected();

private slots:
    void slotUnpauseAllFolders();
    void slotPauseAllFolders();

private:
    void setPauseOnAllFoldersHelper(bool pause);

    static Systray *_instance;
    Systray();

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

    AccessManagerFactory _accessManagerFactory;

    QSet<qlonglong> _callsAlreadyNotified;
};

} // namespace OCC

#endif //SYSTRAY_H
