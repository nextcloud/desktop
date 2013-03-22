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

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#endif

void Systray::showMessage(const QString & title, const QString & message, MessageIcon icon, int millisecondsTimeoutHint)
{
#ifdef USE_FDO_NOTIFICATIONS
    QList<QVariant> args = QList<QVariant>() << "owncloud" << quint32(0) << "owncloud"
                                             << title << message << QStringList () << QVariantMap() << qint32(-1);
    QDBusMessage method = QDBusMessage::createMethodCall("org.freedesktop.Notifications","/org/freedesktop/Notifications", "", "Notify");
    method.setArguments(args);
    QDBusConnection::sessionBus().asyncCall(method);
#else
    QSystemTrayIcon::showMessage(title, message, icon, millisecondsTimeoutHint);
#endif
}
