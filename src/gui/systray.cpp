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

#include <QQmlComponent>
#include <QQmlEngine>

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
    , _accountMenuModel(nullptr)
{
    // Create QML tray engine, build component, set C++ backend context used in window.qml
    // Use pointer instead of engine() helper function until Qt 5.12 is minimum standard
    QQmlEngine *engine = new QQmlEngine;
    _trayComponent = new QQmlComponent(engine, QUrl(QStringLiteral("qrc:/qml/src/gui/tray/window.qml")));
    _trayContext = engine->contextForObject(_trayComponent->create());

    _accountMenuModel = UserModel::instance();

    engine->addImageProvider("avatars", new ImageProvider);
    engine->rootContext()->setContextProperty("systrayBackend", _accountMenuModel);

    // TODO: hack to pass the icon to QML
    //ctxt->setContextProperty("theme", QLatin1String("colored"));
    //ctxt->setContextProperty("filename", "state-offline");

    if (!AccountManager::instance()->accounts().isEmpty()) {
        slotChangeActivityModel(AccountManager::instance()->accounts().first());
    }

    //connect(AccountManager::instance(), &AccountManager::accountAdded,
    //    this, &Systray::slotChangeActivityModel);
    UserModel::instance()->hideWindow();
}

Systray::~Systray()
{
}

void Systray::slotChangeActivityModel(const AccountStatePtr account)
{
    _currentAccount = account;
    emit currentUserChanged();
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

} // namespace OCC
