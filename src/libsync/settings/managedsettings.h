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

enum class SettingSourceType {
    BuiltinDefault,
    PlatformDefault,
    UserConfig,
    PlatformPolicy,
    ServerDefault,
    ServerEnforced,
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

struct ResolvedSetting {
    QString key;
    QVariant value;
    SettingSourceType source = SettingSourceType::BuiltinDefault;
    EnforcementState enforcement = EnforcementState::NotEnforced;
    bool present = false; // false when only the builtin default applied

    [[nodiscard]] bool isEnforced() const { return enforcement == EnforcementState::Enforced; }
};

struct SettingDefinition {
    QString key;
    QVariant builtinDefault;
    bool enforceable = false;
    SettingScope scope = SettingScope::User;
};

class OWNCLOUDSYNC_EXPORT SettingSource
{
public:
    virtual ~SettingSource();

    [[nodiscard]] virtual std::optional<QVariant> read(const QString &key, const QString &group) const = 0;
    [[nodiscard]] virtual SettingSourceType type() const = 0;
    [[nodiscard]] virtual EnforcementState enforcement() const = 0;
    [[nodiscard]] virtual int priority() const = 0;
};

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

    [[nodiscard]] ResolvedSetting resolve(const SettingDefinition &spec, const QString &group = {}) const;

    // Resolves every spec, for a diagnostics export of effective values and sources.
    [[nodiscard]] QList<ResolvedSetting> resolveAll(const QList<SettingDefinition> &specs, const QString &group = {}) const;

private:
    std::vector<std::unique_ptr<SettingSource>> _sources;
};

} // namespace OCC

#endif // MANAGEDSETTINGS_H
