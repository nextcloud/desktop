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

#include "systray.h"
#include "theme.h"
#include "config.h"

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>

namespace {
auto NOTIFICATIONS_SERVICE_C()
{
    return QStringLiteral("org.freedesktop.Notifications");
}

auto NOTIFICATIONS_PATH_C()
{
    return QStringLiteral("/org/freedesktop/Notifications");
}

auto NOTIFICATIONS_IFACE_C()
{
    return QStringLiteral("org.freedesktop.Notifications");
}
}
#endif

namespace OCC {

#ifdef Q_OS_OSX
void *createOsXNotificationCenterDelegate();
void releaseOsXNotificationCenterDelegate(void *delegate);
void sendOsXUserNotification(const QString &title, const QString &message);
#endif

Systray::Systray(QObject *parent)
    : QSystemTrayIcon(parent)
#ifdef Q_OS_OSX
    , delegate(createOsXNotificationCenterDelegate())
#endif
{
}

Systray::~Systray()
{
#ifdef Q_OS_OSX
    if (delegate) {
        releaseOsXNotificationCenterDelegate(delegate);
    }
#endif // Q_OS_OSX
}

void Systray::showMessage(const QString &title, const QString &message, const QIcon &icon, int millisecondsTimeoutHint)
{
#ifdef Q_OS_OSX
    Q_UNUSED(icon)
    Q_UNUSED(millisecondsTimeoutHint)

    sendOsXUserNotification(title, message);
#else
#ifdef USE_FDO_NOTIFICATIONS
    if (QDBusInterface(NOTIFICATIONS_SERVICE_C(), NOTIFICATIONS_PATH_C(), NOTIFICATIONS_IFACE_C()).isValid()) {
        QList<QVariant> args = QList<QVariant>() << Theme::instance()->appNameGUI() << quint32(0) << QStringLiteral(APPLICATION_ICON_NAME)
                                                 << title << message << QStringList() << QVariantMap() << qint32(-1);
        QDBusMessage method = QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE_C(), NOTIFICATIONS_PATH_C(), NOTIFICATIONS_IFACE_C(), QStringLiteral("Notify"));
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon, millisecondsTimeoutHint);
    }
#endif // Q_OS_OSX
}

void Systray::setToolTip(const QString &tip)
{
    QSystemTrayIcon::setToolTip(tr("%1: %2").arg(Theme::instance()->appNameGUI(), tip));
}

} // namespace OCC
