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

namespace OCC {

constexpr auto portalDesktopService = "org.freedesktop.portal.Desktop";
constexpr auto portalDesktopPath = "/org/freedesktop/portal/desktop";

XdgPortal::XdgPortal(QObject *parent)
    : QObject(parent)
    , m_available(false)
{
    initPortalInterface();
}

void XdgPortal::initPortalInterface()
{
    auto *busInterface = QDBusConnection::sessionBus().interface();
    if (!busInterface) {
        qWarning() << "XDG Desktop Portal not available";
        return;
    }

    const auto registered = busInterface->isServiceRegistered(QLatin1String(portalDesktopService));
    m_available = registered.isValid() && registered.value();
    if (!m_available) {
        qWarning() << "XDG Desktop Portal not available";
    }
}

bool XdgPortal::background(const QString &handle_token, const bool &autostart)
{
    if (!m_available) {
        return false;
    }

    QDBusInterface backgroundInterface(
        QLatin1String(portalDesktopService),
        QLatin1String(portalDesktopPath),
        QLatin1String("org.freedesktop.portal.Background"),
        QDBusConnection::sessionBus());
    if (!backgroundInterface.isValid()) { // just in case
        qWarning() << "org.freedesktop.portal.Background not available";
        return false;
    }

    QVariantMap options;
    options[QLatin1String("autostart")] = autostart;
    const QString flatpakId = qEnvironmentVariable("FLATPAK_ID");
    if (flatpakId.isNull()) {
        QStringList list = {Utility::getAppExecutablePath(), QLatin1String("--background")};
        options[QLatin1String("commandline")] = list;
    } else { // when an app is running as a flatpak its "commandline" parameter is handled differently
        QStringList list = {QFileInfo(QCoreApplication::applicationFilePath()).fileName(), QLatin1String("--background")};
        options[QLatin1String("commandline")] = list;
    }

    QDBusMessage reply = backgroundInterface.call(
        QLatin1String("RequestBackground"),
        handle_token,
        options
    );

    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "Background request failed: " << reply.errorMessage();
        return false;
    }

    return true;
}

} // namespace OCC