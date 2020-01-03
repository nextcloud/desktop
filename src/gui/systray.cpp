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

Systray::Systray() // TODO: make singleton, provide ::instance()
    : _currentAccount(nullptr)
    , _trayComponent(nullptr)
    , _trayContext(nullptr)
{
    // Create QML tray engine, build component, set C++ backend context used in window.qml
    // Use pointer instead of engine() helper function until Qt 5.12 is minimum standard
    _trayEngine = new QQmlEngine;
    _trayEngine->addImageProvider("avatars", new ImageProvider);
    _trayEngine->rootContext()->setContextProperty("userModelBackend", UserModel::instance());
    _trayEngine->rootContext()->setContextProperty("systrayBackend", this);
    _trayComponent = new QQmlComponent(_trayEngine, QUrl(QStringLiteral("qrc:/qml/src/gui/tray/window.qml")));
    _trayContext = _trayEngine->contextForObject(_trayComponent->create());

    // TODO: hack to pass the icon to QML
    //ctxt->setContextProperty("theme", QLatin1String("colored"));
    //ctxt->setContextProperty("filename", "state-offline");

    if (!AccountManager::instance()->accounts().isEmpty()) {
        slotChangeActivityModel(AccountManager::instance()->accounts().first());
    }

    hideWindow();
}

Systray::~Systray()
{
}

void Systray::slotChangeActivityModel(const AccountStatePtr account)
{
    _currentAccount = account;
    emit currentUserChanged();
    _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QScreen *trayScreen = QGuiApplication::screenAt(this->geometry().topRight());
#else
    QScreen *trayScreen = QGuiApplication::primaryScreen();
#endif

    // get coordinates from top center point of tray icon
    int trayIconTopCenterX = (this->geometry().topRight() - ((this->geometry().topRight() - this->geometry().topLeft()) * 0.5)).x();
    int trayIconTopCenterY = (this->geometry().topRight() - ((this->geometry().topRight() - this->geometry().topLeft()) * 0.5)).y();

    if ( (trayScreen->geometry().width() - trayIconTopCenterX) < (trayScreen->geometry().width() * 0.5) ) {
        // tray icon is on right side of the screen
        if ( ((trayScreen->geometry().width() - trayIconTopCenterX) < trayScreen->geometry().height() - trayIconTopCenterY)
            && ((trayScreen->geometry().width() - trayIconTopCenterX) < trayIconTopCenterY) ) {
            // taskbar is on the right
            return trayScreen->availableSize().width() - 400 - 6;
        } else {
            // taskbar is on the bottom or top
            if (trayIconTopCenterX - (400 * 0.5) < 0) {
                return 6;
            } else if (trayIconTopCenterX - (400 * 0.5) > trayScreen->geometry().width()) {
                return trayScreen->geometry().width() - 406;
            } else {
                return trayIconTopCenterX - (400 * 0.5);
            }
        }
    } else {
        // tray icon is on left side of the screen
        return (trayScreen->geometry().width() - trayScreen->availableGeometry().width()) + 6;
    }
}
int Systray::calcTrayWindowY()
{
#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
    QScreen *trayScreen = QGuiApplication::screenAt(this->geometry().topRight());
#else
    QScreen *trayScreen = QGuiApplication::primaryScreen();
#endif
    
    // get coordinates from top center point of tray icon
    int trayIconTopCenterX = (this->geometry().topRight() - ((this->geometry().topRight() - this->geometry().topLeft()) * 0.5)).x();
    int trayIconTopCenterY = (this->geometry().topRight() - ((this->geometry().topRight() - this->geometry().topLeft()) * 0.5)).y();

    if ( (trayScreen->geometry().height() - trayIconTopCenterY) < (trayScreen->geometry().height() * 0.5) ) {
        // tray icon is on bottom side of the screen
        if ( ((trayScreen->geometry().height() - trayIconTopCenterY) < trayScreen->geometry().width() - trayIconTopCenterX )
            && ((trayScreen->geometry().height() - trayIconTopCenterY) < trayIconTopCenterX) ) {
            // taskbar is on the bottom
            return trayScreen->availableSize().height() - 500 - 6;
        } else {
            // taskbar is on the right or left
            if (trayIconTopCenterY - 500 > 0) {
                return trayIconTopCenterY - 500;
            } else {
                return 6;
            }
        }
    } else {
        // tray icon is on the top
        return (trayScreen->geometry().height() - trayScreen->availableGeometry().height()) + 6;
    }
}


} // namespace OCC
