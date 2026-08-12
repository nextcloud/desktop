/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MANAGEDSETTINGS_H
#define MANAGEDSETTINGS_H

#include <QString>
#include <QVariant>

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
    ServerLocked, // phase 2
};

enum class LockState {
    Unlocked,
    Locked,
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
    LockState lockState = LockState::Unlocked;
    bool present = false; // false when only the builtin default applied

    [[nodiscard]] bool isLocked() const { return lockState == LockState::Locked; }
};

struct SettingSpec {
    QString key;
    QVariant builtinDefault;
    bool lockable = false;
    SettingScope scope = SettingScope::User;
};

class OWNCLOUDSYNC_EXPORT SettingSource
{
public:
    virtual ~SettingSource();

    // std::nullopt means the source does not define key.
    [[nodiscard]] virtual std::optional<QVariant> read(const QString &key, const QString &group) const = 0;
    [[nodiscard]] virtual SettingSourceKind kind() const = 0;
    [[nodiscard]] virtual LockState lockState() const = 0;
    [[nodiscard]] virtual int priority() const = 0;
};

// Resolves the effective value of a setting: locked policy, then user config,
// then the highest default, then the builtin default.
class OWNCLOUDSYNC_EXPORT ManagedSettings
{
public:
    void addSource(std::unique_ptr<SettingSource> source);

    [[nodiscard]] ManagedValue resolve(const SettingSpec &spec, const QString &group = {}) const;

private:
    std::vector<std::unique_ptr<SettingSource>> _sources;
};

} // namespace OCC

#endif // MANAGEDSETTINGS_H
