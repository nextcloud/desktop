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

// Server delivery flow: see README.md

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
    ServerSettingsSource(QVariantMap values, SettingSourceType type, EnforcementState enforcement, int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceType type() const override;
    [[nodiscard]] EnforcementState enforcement() const override;
    [[nodiscard]] int priority() const override;

private:
    QVariantMap _values;
    SettingSourceType _type;
    EnforcementState _enforcement;
    int _priority;
};

// Build the server default (priority 30) and server enforced (priority 100) sources
// from already sanitized settings. Empty maps produce no source.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildServerSources(const ServerManagedSettings &sanitized);

} // namespace OCC

#endif // SERVERMANAGEDSETTINGS_H
