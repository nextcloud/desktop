/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDSETTINGS_H
#define MANAGEDSETTINGS_H

#include <QString>
#include <QVariant>
#include <QList>

#include <memory>
#include <optional>
#include <vector>

#include "owncloudlib.h"

namespace OCC {

enum class SettingSourceKind {
    BuiltinDefault,
    PlatformDefault,
    UserConfig,
    PlatformPolicy,
    ServerDefault, // phase 2
    ServerEnforced, // phase 2
};

enum class EnforcementState {
    NotEnforced,
    Enforced,
};

enum class SettingScope {
    Device,
    User,
    Account,
    Folder,
};

struct ManagedValue {
    QString key;
    QVariant value;
    SettingSourceKind source = SettingSourceKind::BuiltinDefault;
    EnforcementState enforcement = EnforcementState::NotEnforced;
    bool present = false; // false when only the builtin default applied

    [[nodiscard]] bool isEnforced() const { return enforcement == EnforcementState::Enforced; }
};

struct SettingSpec {
    QString key;
    QVariant builtinDefault;
    bool enforceable = false;
    SettingScope scope = SettingScope::User;
};

class OWNCLOUDSYNC_EXPORT SettingSource
{
public:
    virtual ~SettingSource();

    // std::nullopt means the source does not define key.
    [[nodiscard]] virtual std::optional<QVariant> read(const QString &key, const QString &group) const = 0;
    [[nodiscard]] virtual SettingSourceKind kind() const = 0;
    [[nodiscard]] virtual EnforcementState enforcement() const = 0;
    [[nodiscard]] virtual int priority() const = 0;
};

/**
 * How a managed setting is resolved (e.g. skipUpdateCheck):
 *
 * config.php (admin)                                     [server, optional]
 *   |
 *   support app Capabilities::getCapabilities
 *   |   allow list filter, enterprise subscription gate
 *   |
 *   OCS: support.desktopClient { defaults, enforced }
 *   |
 * Account::setCapabilities                               [client]
 *   |
 *   Account::updateServerManagedSettings
 *   |   Capabilities::desktopClientManagedSettings then parseServerManagedSettings
 *   |   sanitizeServerManagedSettings  (client allow list, drops non enforceable)
 *   |
 *   AccountManager::updateServerManagedSettings
 *   |   merge subscribed accounts (the subscribed account wins)
 *   |
 *   ConfigFile::setServerManagedSettings   (JSON in .cfg, offline cache)
 *   |
 * ConfigFile::skipUpdateCheck / autoUpdateCheck          [read]
 *   |
 *   ConfigFile::resolveManagedBool
 *     |   add the sources for this key:
 *     |   buildDeviceSources()   Windows GP / macOS forced (enforced), OS default
 *     |   UserConfigSource       the user .cfg
 *     |   buildServerSources()   server enforced, server default
 *     |
 *     ManagedSettings::resolve(spec)
 *       |   highest precedence level wins, ties broken by source priority:
 *       |
 *       device enforced (200) > server enforced (100) > user (50)
 *                           > server default (30) > device default (20) > builtin
 *       |
 *   ManagedValue { value, source, enforced/default }       [return]
 */
class OWNCLOUDSYNC_EXPORT ManagedSettings
{
public:
    ManagedSettings() = default;
    ~ManagedSettings() = default;

    ManagedSettings(const ManagedSettings &) = delete;
    ManagedSettings &operator=(const ManagedSettings &) = delete;
    ManagedSettings(ManagedSettings &&) = default;
    ManagedSettings &operator=(ManagedSettings &&) = default;

    void addSource(std::unique_ptr<SettingSource> source);

    [[nodiscard]] ManagedValue resolve(const SettingSpec &spec, const QString &group = {}) const;

    // Resolves every spec, for a diagnostics export of effective values and sources.
    [[nodiscard]] QList<ManagedValue> resolveAll(const QList<SettingSpec> &specs, const QString &group = {}) const;

private:
    std::vector<std::unique_ptr<SettingSource>> _sources;
};

} // namespace OCC

#endif // MANAGEDSETTINGS_H
