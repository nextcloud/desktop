/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef XDG_PORTAL_H
#define XDG_PORTAL_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtDBus/QDBusConnection>

namespace OCC {

/**
 * @brief XDG Desktop Portal interface for Qt D-Bus
 *
 * Abstracts D-Bus calls behind simpler methods.
 */
class XdgPortal : public QObject
{
    Q_OBJECT

public:
    explicit XdgPortal(QObject *parent = nullptr);

    /**
     * @brief Check if XDPs are available
     * @return true if available, false if not
     */
    bool isAvailable() const;

    /**
     * @brief Requests that the application is allowed to run in the background; see https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Background.html
     * @param autostart true if the app also wants to be started automatically at login.
     * @return True if successful
     */
    bool background(bool autostart);

private:
    QDBusConnection _connection;
};

} // namespace OCC

#endif // XDG_PORTAL_H