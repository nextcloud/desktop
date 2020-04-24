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
#include "tray/UserModel.h"

class QQmlApplicationEngine;

namespace OCC {

#ifdef Q_OS_OSX
bool canOsXSendUserNotification();
void sendOsXUserNotification(const QString &title, const QString &message);
#endif

namespace Ui {
    class Systray;
}

enum TaskBarPosition {
    Bottom = 0,
    Left,
    Top,
    Right
};

/**
 * @brief The Systray class
 * @ingroup gui
 */
class Systray
    : public QSystemTrayIcon
{
    Q_OBJECT
public:
    static Systray *instance();
    virtual ~Systray() {};

    void create();
    void showMessage(const QString &title, const QString &message, MessageIcon icon = Information);
    void setToolTip(const QString &tip);
    bool isOpen();

    Q_INVOKABLE void pauseResumeSync();
    Q_INVOKABLE bool syncIsPaused();
    Q_INVOKABLE void setOpened();
    Q_INVOKABLE void setClosed();
    Q_INVOKABLE int screenIndex();
    Q_INVOKABLE QPoint calcTrayIconCenter();
    Q_INVOKABLE int taskbarOrientation();
    Q_INVOKABLE QRect taskbarRect();

signals:
    void currentUserChanged();
    void openSettings();
    void openHelp();
    void shutdown();
    void pauseSync();
    void resumeSync();

    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void openShareDialog(const QString &sharePath, const QString &localPath);

public slots:
    void slotNewUserSelected();

private:
    static Systray *_instance;
    Systray();
    bool _isOpen = false;
    bool _syncIsPaused = false;
    QQmlApplicationEngine *_trayEngine;
};

} // namespace OCC

#endif //SYSTRAY_H
