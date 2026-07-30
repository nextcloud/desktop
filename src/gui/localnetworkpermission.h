/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LOCALNETWORKPERMISSION_H
#define LOCALNETWORKPERMISSION_H

#include <QCoreApplication>
#include <QUrl>

#include <functional>

class QObject;

namespace OCC::LocalNetworkPermission {

/**
 * Checks whether Local Network permission denied a failed connection.
 *
 * The callback receives false when the platform cannot determine the denial.
 */
void checkDeniedForConnection(const QUrl &url, QObject *context, std::function<void(bool)> callback);

/** Returns an actionable error message for a denied local network connection. */
QString deniedError();

} // namespace OCC::LocalNetworkPermission

#endif // LOCALNETWORKPERMISSION_H
