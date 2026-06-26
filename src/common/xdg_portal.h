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
    bool isAvailable();

    /**
     * @brief Requests that the application is allowed to run in the background; see https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Background.html
     * @param handle_token A string that will be used as the last element of the handle. Must be a valid object path element. See the [Request](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html#org-freedesktop-portal-request) documentation for more information about the handle.
     * @param autostart true if the app also wants to be started automatically at login.
     * @return True if successful
     */
    bool background(const QString &handle_token, const bool &autostart);

private:
    void initPortalInterface();

    QDBusConnection _connection;
    bool _available = false;
};

} // namespace OCC

#endif // XDG_PORTAL_H