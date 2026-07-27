/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LOCALNETWORKPERMISSION_H
#define LOCALNETWORKPERMISSION_H

#include <QString>
#include <QUrl>

#include <functional>

class QObject;

namespace OCC::Mac {

/** Returns whether the connection-specific check is available on this macOS version. */
bool localNetworkPermissionCheckAvailable();

/**
 * Checks whether macOS denied local network access for a failed connection.
 *
 * The callback runs on @p context's thread. It receives false on macOS before
 * version 15, when the destination is not local, or when the denial cannot be
 * determined.
 */
void checkLocalNetworkPermissionDeniedForConnection(const QUrl &url, QObject *context,
                                                    std::function<void(bool)> callback);

/** Returns an actionable error message for a denied local network connection. */
QString localNetworkPermissionDeniedError();

} // namespace OCC::Mac

#endif // LOCALNETWORKPERMISSION_H
