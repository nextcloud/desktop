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

#include "accountmanager.h"
#include "systray.h"
#include "theme.h"
#include "config.h"
#include "tray/UserModel.h"

#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QScreen>

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#define NOTIFICATIONS_SERVICE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"
#define NOTIFICATIONS_IFACE "org.freedesktop.Notifications"
#endif

namespace OCC {

Systray *Systray::_instance = nullptr;

Systray *Systray::instance()
{
    if (_instance == nullptr) {
        _instance = new Systray();
    }
    return _instance;
}

Systray::Systray()
    : _isOpen(false)
    , _syncIsPaused(false)
    , _trayComponent(nullptr)
    , _trayContext(nullptr)
{
    // Create QML tray engine, build component, set C++ backend context used in window.qml
    // Use pointer instead of engine() helper function until Qt 5.12 is minimum standard
    _trayEngine = new QQmlEngine;
    _trayEngine->addImageProvider("avatars", new ImageProvider);
    _trayEngine->rootContext()->setContextProperty("userModelBackend", UserModel::instance());
    _trayEngine->rootContext()->setContextProperty("appsMenuModelBackend", UserAppsModel::instance());
    _trayEngine->rootContext()->setContextProperty("systrayBackend", this);

    _trayComponent = new QQmlComponent(_trayEngine, QUrl(QStringLiteral("qrc:/qml/src/gui/tray/Window.qml")));

    connect(UserModel::instance(), &UserModel::newUserSelected,
        this, &Systray::slotNewUserSelected);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Systray::showWindow);
}

void Systray::create()
{
    if (_trayContext == nullptr) {
        if (!AccountManager::instance()->accounts().isEmpty()) {
            _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
        }
        _trayContext = _trayEngine->contextForObject(_trayComponent->create());
        hideWindow();
    }
}

void Systray::slotNewUserSelected()
{
    // Change ActivityModel
    _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());

    // Rebuild App list
    UserAppsModel::instance()->buildAppList();
}

bool Systray::isOpen()
{
    return _isOpen;
}

Q_INVOKABLE void Systray::setOpened()
{
    _isOpen = true;
}

Q_INVOKABLE void Systray::setClosed()
{
    _isOpen = false;
}

void Systray::showMessage(const QString &title, const QString &message, MessageIcon icon, int millisecondsTimeoutHint)
{
#ifdef USE_FDO_NOTIFICATIONS
    if (QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        QList<QVariant> args = QList<QVariant>() << APPLICATION_NAME << quint32(0) << APPLICATION_ICON_NAME
                                                 << title << message << QStringList() << QVariantMap() << qint32(-1);
        QDBusMessage method = QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE, "Notify");
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
#ifdef Q_OS_OSX
        if (canOsXSendUserNotification()) {
        sendOsXUserNotification(title, message);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon, millisecondsTimeoutHint);
    }
}

void Systray::setToolTip(const QString &tip)
{
    QSystemTrayIcon::setToolTip(tr("%1: %2").arg(Theme::instance()->appNameGUI(), tip));
}

int Systray::calcTrayWindowX()
{
#ifdef Q_OS_OSX
    // macOS handles DPI awareness differently
    // and menu bar is always at the top, icons starting from the right

    QPoint topLeft = this->geometry().topLeft();
    QPoint topRight = this->geometry().topRight();
    int trayIconTopCenterX = (topRight - ((topRight - topLeft) * 0.5)).x();
    return trayIconTopCenterX - (400 * 0.5);
#else
    QScreen *trayScreen = QGuiApplication::primaryScreen();
    int screenWidth = trayScreen->geometry().width();
    int screenHeight = trayScreen->geometry().height();
    int availableWidth = trayScreen->availableGeometry().width();
    int availableHeight = trayScreen->availableGeometry().height();
    QPoint topRightDpiAware = this->geometry().topRight() / trayScreen->devicePixelRatio();
    QPoint topLeftDpiAware = this->geometry().topLeft() / trayScreen->devicePixelRatio();

    // get coordinates from top center point of tray icon
    int trayIconTopCenterX = (topRightDpiAware - ((topRightDpiAware - topLeftDpiAware) * 0.5)).x();
    int trayIconTopCenterY = (topRightDpiAware - ((topRightDpiAware - topLeftDpiAware) * 0.5)).y();

    if (availableHeight < screenHeight) {
        // taskbar is on top or bottom
        if (trayIconTopCenterX + (400 * 0.5) > availableWidth) {
            return availableWidth - 400 - 12;
        } else {
            return trayIconTopCenterX - (400 * 0.5);
        }
    } else {
        if (trayScreen->availableGeometry().x() > trayScreen->geometry().x()) {
            // on the left
            return (screenWidth - availableWidth) + 6;
        } else {
            // on the right
            return screenWidth - 400 - (screenWidth - availableWidth) - 6;
        }
    }
#endif
}
int Systray::calcTrayWindowY()
{
#ifdef Q_OS_OSX
    // macOS menu bar is always 22 (effective) pixels
    // don't use availableGeometry() here, because this also excludes the dock
    return 22+6;
#else
    QScreen *trayScreen = QGuiApplication::primaryScreen();
    int screenWidth = trayScreen->geometry().width();
    int screenHeight = trayScreen->geometry().height();
    int availableHeight = trayScreen->availableGeometry().height();
    QPoint topRightDpiAware = this->geometry().topRight() / trayScreen->devicePixelRatio();
    QPoint topLeftDpiAware = this->geometry().topLeft() / trayScreen->devicePixelRatio();

    // get coordinates from top center point of tray icon
    int trayIconTopCenterX = (topRightDpiAware - ((topRightDpiAware - topLeftDpiAware) * 0.5)).x();
    int trayIconTopCenterY = (topRightDpiAware - ((topRightDpiAware - topLeftDpiAware) * 0.5)).y();

    if (availableHeight < screenHeight) {
        // taskbar is on top or bottom
        if (trayScreen->availableGeometry().y() > trayScreen->geometry().y()) {
            // on top
            return (screenHeight - availableHeight) + 6;
        } else {
            // on bottom
            return screenHeight - 510 - (screenHeight - availableHeight) - 6;
        }
    } else {
        // on the left or right
        return (trayIconTopCenterY - 510 + 12);
    }
#endif
}

bool Systray::syncIsPaused()
{
    return _syncIsPaused;
}

void Systray::pauseResumeSync()
{
    if (_syncIsPaused) {
        _syncIsPaused = false;
        emit resumeSync();
    } else {
        _syncIsPaused = true;
        emit pauseSync();
    }
}

} // namespace OCC
