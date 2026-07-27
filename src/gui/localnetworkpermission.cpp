/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "localnetworkpermission.h"

namespace OCC::LocalNetworkPermission {

void checkDeniedForConnection(const QUrl &url, QObject *context, std::function<void(bool)> callback)
{
    Q_UNUSED(url)

    if (context) {
        callback(false);
    }
}

QString deniedError()
{
    return QCoreApplication::translate("LocalNetworkPermission",
                                       "Local Network access is disabled. Enable it to connect to the server.");
}

} // namespace OCC::LocalNetworkPermission
