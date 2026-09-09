/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDCONFIG_H
#define MANAGEDCONFIG_H

#include <QReadWriteLock>
#include <QString>

#include "owncloudlib.h"
#include "settings/servermanagedsettings.h"

namespace OCC {

// Caches parsed server settings in memory to avoid reparsing the config file on every read. Thread safe.
class OWNCLOUDSYNC_EXPORT ManagedConfig
{
public:
    static ManagedConfig &instance();

    [[nodiscard]] ServerManagedSettings serverSettings(const QString &configFilePath);
    void setServerSettings(const QString &configFilePath, const ServerManagedSettings &settings);
    void invalidate();

private:
    ManagedConfig() = default;
    [[nodiscard]] static ServerManagedSettings parse(const QString &configFilePath);
    static void save(const QString &configFilePath, const ServerManagedSettings &settings);

    QReadWriteLock _lock;
    bool _loaded = false;
    QString _configFilePath;
    ServerManagedSettings _cached;
};

} // namespace OCC

#endif // MANAGEDCONFIG_H
