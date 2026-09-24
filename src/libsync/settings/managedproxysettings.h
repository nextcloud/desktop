/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDPROXYSETTINGS_H
#define MANAGEDPROXYSETTINGS_H

#include <QString>

namespace OCC
{

// Fields without a flag keep the account's value.
struct ManagedProxySettings {
    bool isManaged = false;
    bool isEnforced = false;
    bool typeManaged = false;
    bool hostManaged = false;
    bool portManaged = false;
    bool typeEnforced = false;
    bool hostEnforced = false;
    bool portEnforced = false;
    int proxyType = 0;
    QString proxyHostName;
    int proxyPort = 0;
};

} // namespace OCC

#endif // MANAGEDPROXYSETTINGS_H
