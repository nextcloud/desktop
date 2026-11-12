/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "xdg_portal.h"
#include "utility.h"
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QDebug>
#include <QFileInfo>
#include <QCoreApplication>
#include <QUuid>

namespace OCC {

constexpr auto portalDesktopService = "org.freedesktop.portal.Desktop";
constexpr auto portalDesktopPath = "/org/freedesktop/portal/desktop";

XdgPortal::XdgPortal(QObject *parent)
    : QObject(parent),
      _connection(QDBusConnection::sessionBus())
{
    if (!isAvailable()) {
        qWarning() << "XDG Desktop Portal not available";
    }
}

bool XdgPortal::isAvailable() const
{
    // do a basic ping to check if the portal is active
    QDBusInterface iface(
        QLatin1String(portalDesktopService),
        QLatin1String(portalDesktopPath),
        QLatin1String("org.freedesktop.DBus.Peer"),
        _connection
    );

    QDBusReply<void> reply = iface.call(QLatin1String("Ping"));

    return reply.isValid();
}

bool XdgPortal::background(bool autostart)
{
    if (!isAvailable()) {
        return false;
    }

    QDBusInterface backgroundInterface(
        QLatin1String(portalDesktopService),
        QLatin1String(portalDesktopPath),
        QLatin1String("org.freedesktop.portal.Background"),
        _connection);
    if (!backgroundInterface.isValid()) { // just in case
        qWarning() << "org.freedesktop.portal.Background not available";
        return false;
    }

    const QString handle_token = QUuid::createUuid().toString(QUuid::Id128);

    QVariantMap options;
    options[QLatin1String("autostart")] = autostart;
    options[QLatin1String("handle_token")] = handle_token;
    const QString flatpakId = qEnvironmentVariable("FLATPAK_ID");
    if (flatpakId.isNull()) {
        QStringList list = {Utility::getAppExecutablePath(), QLatin1String("--background")};
        options[QLatin1String("commandline")] = list;
    } else { // when an app is running as a flatpak its "commandline" parameter is handled differently
        QStringList list = {QFileInfo(QCoreApplication::applicationFilePath()).fileName(), QLatin1String("--background")};
        options[QLatin1String("commandline")] = list;
    }

    QDBusReply<QDBusObjectPath> reply = backgroundInterface.call(
        QLatin1String("RequestBackground"),
        QLatin1String(""),
        options
    );

    if (!reply.isValid()) {
        qWarning() << "Background request failed: " << reply.error().name() << reply.error().message();
        return false;
    }

    return true;
}

} // namespace OCC