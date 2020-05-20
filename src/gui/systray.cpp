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
#include "common/utility.h"
#include "tray/UserModel.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
    if (!_instance) {
        _instance = new Systray();
    }
    return _instance;
}

Systray::Systray()
    : _trayEngine(new QQmlApplicationEngine(this))
{
    _trayEngine->addImportPath("qrc:/qml/theme");
    _trayEngine->addImageProvider("avatars", new ImageProvider);
    _trayEngine->rootContext()->setContextProperty("userModelBackend", UserModel::instance());
    _trayEngine->rootContext()->setContextProperty("appsMenuModelBackend", UserAppsModel::instance());
    _trayEngine->rootContext()->setContextProperty("systrayBackend", this);

    connect(UserModel::instance(), &UserModel::newUserSelected,
        this, &Systray::slotNewUserSelected);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Systray::showWindow);
}

void Systray::create()
{
    if (!AccountManager::instance()->accounts().isEmpty()) {
        _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
    }
    _trayEngine->load(QStringLiteral("qrc:/qml/src/gui/tray/Window.qml"));
    hideWindow();
    emit activated(QSystemTrayIcon::ActivationReason::Unknown);
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

void Systray::showMessage(const QString &title, const QString &message, MessageIcon icon)
{
#ifdef USE_FDO_NOTIFICATIONS
    if (QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        const QVariantMap hints = {{QStringLiteral("desktop-entry"), LINUX_APPLICATION_ID}};
        QList<QVariant> args = QList<QVariant>() << APPLICATION_NAME << quint32(0) << APPLICATION_ICON_NAME
                                                 << title << message << QStringList() << hints << qint32(-1);
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
        QSystemTrayIcon::showMessage(title, message, icon);
    }
}

void Systray::setToolTip(const QString &tip)
{
    QSystemTrayIcon::setToolTip(tr("%1: %2").arg(Theme::instance()->appNameGUI(), tip));
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

/********************************************************************************************/
/* Helper functions for cross-platform tray icon position and taskbar orientation detection */
/********************************************************************************************/

QScreen *Systray::currentScreen() const
{
    const auto screens = QGuiApplication::screens();
    const auto cursorPos = QCursor::pos();

    for (const auto screen : screens) {
        if (screen->geometry().contains(cursorPos)) {
            return screen;
        }
    }

    return nullptr;
}

QVariant Systray::currentScreenVar() const
{
    return QVariant::fromValue(currentScreen());
}

Systray::TaskBarPosition Systray::taskbarOrientation() const
{
// macOS: Always on top
#if defined(Q_OS_MACOS)
    return TaskBarPosition::Top;
// Windows: Check registry for actual taskbar orientation
#elif defined(Q_OS_WIN)
    auto taskbarPosition = Utility::registryGetKeyValue(HKEY_CURRENT_USER,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StuckRects3",
        "Settings");
    switch (taskbarPosition.toInt()) {
    // Mapping windows binary value (0 = left, 1 = top, 2 = right, 3 = bottom) to qml logic (0 = bottom, 1 = left...)
    case 0:
        return TaskBarPosition::Left;
    case 1:
        return TaskBarPosition::Top;
    case 2:
        return TaskBarPosition::Right;
    case 3:
        return TaskBarPosition::Bottom;
    default:
        return TaskBarPosition::Bottom;
    }
// Probably Linux
#else
    const auto screenRect = currentScreenRect();
    const auto trayIconCenter = calcTrayIconCenter();

    auto distBottom = screenRect.bottom() - trayIconCenter.y();
    auto distRight = screenRect.right() - trayIconCenter.x();
    auto distLeft = trayIconCenter.x() - screenRect.left();
    auto distTop = trayIconCenter.y() - screenRect.top();

    if (distBottom < distRight && distBottom < distTop && distBottom < distLeft) {
        return TaskBarPosition::Bottom;
    } else if (distLeft < distTop && distLeft < distRight && distLeft < distBottom) {
        return TaskBarPosition::Left;
    } else if (distTop < distRight && distTop < distBottom && distTop < distLeft) {
        return TaskBarPosition::Top;
    } else {
        return TaskBarPosition::Right;
    }
#endif
}

// TODO: Get real taskbar dimensions Linux as well
QRect Systray::taskbarGeometry() const
{
#if defined(Q_OS_WIN)
    QRect tbRect = Utility::getTaskbarDimensions();
    //QML side expects effective pixels, convert taskbar dimensions if necessary
    auto pixelRatio = currentScreen()->devicePixelRatio();
    if (pixelRatio != 1) {
        tbRect.setHeight(tbRect.height() / pixelRatio);
        tbRect.setWidth(tbRect.width() / pixelRatio);
    }
    return tbRect;
#elif defined(Q_OS_MACOS)
    // Finder bar is always 22px height on macOS (when treating as effective pixels)
    auto screenWidth = currentScreenRect().width();
    return QRect(0, 0, screenWidth, 22);
#else
    if (taskbarOrientation() == TaskBarPosition::Bottom || taskbarOrientation() == TaskBarPosition::Top) {
        auto screenWidth = currentScreenRect().width();
        return QRect(0, 0, screenWidth, 32);
    } else {
        auto screenHeight = currentScreenRect().height();
        return QRect(0, 0, 32, screenHeight);
    }
#endif
}

QRect Systray::currentScreenRect() const
{
    const auto screen = currentScreen();
    const auto rect = screen->geometry();
    return rect.translated(screen->virtualGeometry().topLeft());
}

QPoint Systray::computeWindowReferencePoint(int width, int height) const
{
    const auto trayIconCenter = calcTrayIconCenter();
    const auto taskbarRect = taskbarGeometry();
    const auto taskbarScreenEdge = taskbarOrientation();
    const auto screenRect = currentScreenRect();

    switch(taskbarScreenEdge) {
    case TaskBarPosition::Bottom:
        return {
            trayIconCenter.x() - width / 2,
            screenRect.bottom() - taskbarRect.height() - height - 4
        };
    case TaskBarPosition::Left:
        return {
            screenRect.left() + taskbarRect.width() + 4,
            trayIconCenter.y()
        };
    case TaskBarPosition::Top:
        return {
            trayIconCenter.x() - width / 2,
            screenRect.top() + taskbarRect.height() + 4
        };
    case TaskBarPosition::Right:
        return {
            screenRect.right() - taskbarRect.width() - width - 4,
            trayIconCenter.y()
        };
    }
    Q_UNREACHABLE();
}

QPoint Systray::computeWindowPosition(int width, int height) const
{
    auto referencePoint = computeWindowReferencePoint(width, height);

    const auto taskbarScreenEdge = taskbarOrientation();
    const auto taskbarRect = taskbarGeometry();
    const auto screenRect = currentScreenRect();

    if (screenRect.right() <= referencePoint.x() + width) {
        referencePoint.rx() = screenRect.right() - width - 4;
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        referencePoint.rx() -= taskbarScreenEdge == TaskBarPosition::Right ? taskbarRect.width() : 0;
#endif
    }

    if (referencePoint.x() <= screenRect.left()) {
        referencePoint.rx() = screenRect.left() + 4;
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        referencePoint.rx() += taskbarScreenEdge == TaskBarPosition::Left ? taskbarRect.width() : 0;
#endif
    }

    if (referencePoint.y() <= screenRect.top()) {
        referencePoint.ry() = screenRect.top() + 4;

#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
        referencePoint.ry() += taskbarScreenEdge == TaskBarPosition::Top ? taskbarRect.height() : 0;
#endif
    }
    if (screenRect.bottom() <= referencePoint.y() + height) {
        referencePoint.ry() = screenRect.bottom() - height - 4;
    }

    return referencePoint;
}

QPoint Systray::calcTrayIconCenter() const
{
    // QSystemTrayIcon::geometry() is broken for ages on most Linux DEs (invalid geometry returned)
    // thus we can use this only for Windows and macOS
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    auto trayIconCenter = geometry().center();
    return trayIconCenter;
#else
    // On Linux, fall back to mouse position (assuming tray icon is activated by mouse click)
    return QCursor::pos();
#endif
}

} // namespace OCC
