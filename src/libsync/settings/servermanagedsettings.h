/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SERVERMANAGEDSETTINGS_H
#define SERVERMANAGEDSETTINGS_H

#include <QString>
#include <QVariantMap>

#include <memory>
#include <optional>
#include <vector>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC {

/**
 * Server delivered managed settings: from the admin's config.php to a source the
 * resolver can read. See ManagedSettings for the full resolution flow.
 *
 * [server, support app]                     (separate repo, enterprise gated)
 *   config.php: desktopclient.defaults / .enforced
 *     |
 *   DesktopClientSettingsService   allow list, never secrets
 *     |
 *   Capabilities: support.desktopClient { schemaVersion, defaults, enforced }
 *     |
 *   OCS  /cloud/capabilities
 *     |
 * [client]
 *   Account::setCapabilities
 *     |
 *   Capabilities::desktopClientManagedSettings
 *     |   parseServerManagedSettings   (capability map into ServerManagedSettings)
 *     |
 *   sanitizeServerManagedSettings
 *     |   client allow list: drop unknown keys,
 *     |   keep only server enforceable keys in enforced
 *     |
 *   AccountManager::updateServerManagedSettings
 *     |   merge subscribed accounts, the subscribed account wins
 *     |
 *   ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
 *     |
 *   buildServerSources
 *     |   ServerSettingsSource  enforced    (ServerEnforced, priority 100)
 *     |   ServerSettingsSource  defaults  (ServerDefault, priority 30)
 *     |
 *   [added to the resolver by ConfigFile::resolveManagedBool]
 */

// Managed settings delivered by the server through the support.desktopClient
// capability. defaults are suggestions; enforced values cannot be changed.
struct ServerManagedSettings {
    int schemaVersion = 0;
    QVariantMap defaults;
    QVariantMap enforced;
};

// Parse the support.desktopClient capability submap into ServerManagedSettings.
[[nodiscard]] OWNCLOUDSYNC_EXPORT ServerManagedSettings parseServerManagedSettings(const QVariantMap &desktopClientCapability);

// Apply the client allow list: accepted keys in defaults, accepted server
// enforceable keys in enforced. The control point for server input.
[[nodiscard]] OWNCLOUDSYNC_EXPORT ServerManagedSettings sanitizeServerManagedSettings(const ServerManagedSettings &raw);

// A source backed by an in memory map of sanitized server values.
class OWNCLOUDSYNC_EXPORT ServerSettingsSource : public SettingSource
{
public:
    ServerSettingsSource(QVariantMap values, SettingSourceKind kind, EnforcementState enforcement, int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceKind kind() const override;
    [[nodiscard]] EnforcementState enforcement() const override;
    [[nodiscard]] int priority() const override;

private:
    QVariantMap _values;
    SettingSourceKind _kind;
    EnforcementState _enforcement;
    int _priority;
};

// Build the server default (priority 30) and server enforced (priority 100) sources
// from already sanitized settings. Empty maps produce no source.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildServerSources(const ServerManagedSettings &sanitized);

} // namespace OCC

#endif // SERVERMANAGEDSETTINGS_H
