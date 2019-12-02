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
#include "../common/utility.h"
#include "theme.h"
#include "config.h"

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
    , _trayContext(nullptr)
{
    // Create QML tray engine, build component, set C++ backend context used in window.qml
    QQmlEngine *engine = new QQmlEngine;
    QQmlComponent systray(engine, QUrl(QStringLiteral("qrc:/qml/src/gui/tray/init.qml")));
    _trayContext = engine->contextForObject(systray.create());

    systray.engine()->rootContext()->setContextProperty("systrayBackend", this);

    // TODO: hack to pass the icon to QML
    //ctxt->setContextProperty("theme", QLatin1String("colored"));
    //ctxt->setContextProperty("filename", "state-offline");

    if (!AccountManager::instance()->accounts().isEmpty()) {

        slotChangeActivityModel(AccountManager::instance()->accounts().first());
    }

    //_trayContext->setContextProperty("serverTest", QVariant("Test"));
    //connect(AccountManager::instance(), &AccountManager::accountAdded,
    //    this, &Systray::slotChangeActivityModel);
}

Systray::~Systray()
{
}

Q_INVOKABLE QIcon Systray::currentAvatar() const
{
    QImage userAvatarImg = _currentAccount->account()->avatar();
    QIcon userAvatar(QPixmap::fromImage(userAvatarImg));
    return userAvatar;
}

Q_INVOKABLE QString Systray::currentAccountServer() const
{
    QString serverUrl = _currentAccount->account()->url().toString();
    if (serverUrl.length() > 25) {
        serverUrl.truncate(23);
        serverUrl.append(QByteArray("..."));
    }
    return serverUrl;
}

Q_INVOKABLE QString Systray::currentAccountUser() const
{
    QString userName = _currentAccount->account()->davDisplayName();
    if (userName.length() > 19) {
        userName.truncate(17);
        userName.append(QByteArray("..."));
    }
    return userName;
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
