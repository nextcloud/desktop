/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/settingsources.h"

#include <QSettings>

#include <utility>

namespace OCC {

UserConfigSource::UserConfigSource(QString configFilePath, QString group, int priority)
    : _configFilePath(std::move(configFilePath))
    , _group(std::move(group))
    , _priority(priority)
{
}

std::optional<QVariant> UserConfigSource::read(const QString &key, const QString &group) const
{
    const auto effectiveGroup = _group.isEmpty() ? group : _group;
    QSettings settings(_configFilePath, QSettings::IniFormat);
    if (!effectiveGroup.isEmpty()) {
        settings.beginGroup(effectiveGroup);
    }
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    return settings.value(key);
}

SettingSourceType UserConfigSource::type() const
{
    return SettingSourceType::UserConfig;
}

EnforcementState UserConfigSource::enforcement() const
{
    return EnforcementState::NotEnforced;
}

int UserConfigSource::priority() const
{
    return _priority;
}

NativeSettingsSource::NativeSettingsSource(QString location, SettingSourceType type, EnforcementState enforcement, int priority)
    : _location(std::move(location))
    , _type(type)
    , _enforcement(enforcement)
    , _priority(priority)
{
}

std::optional<QVariant> NativeSettingsSource::read(const QString &key, const QString &group) const
{
    QSettings settings(_location, QSettings::NativeFormat);
    if (!group.isEmpty()) {
        settings.beginGroup(group);
    }
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    return settings.value(key);
}

SettingSourceType NativeSettingsSource::type() const
{
    return _type;
}

EnforcementState NativeSettingsSource::enforcement() const
{
    return _enforcement;
}

int NativeSettingsSource::priority() const
{
    return _priority;
}

ForcedPreferenceSource::ForcedPreferenceSource(int priority)
    : _priority(priority)
{
}

std::optional<QVariant> ForcedPreferenceSource::read(const QString &key, const QString &) const
{
    if (!isForced(key)) {
        return std::nullopt;
    }
    return copyForcedValue(key);
}

SettingSourceType ForcedPreferenceSource::type() const
{
    return SettingSourceType::PlatformPolicy;
}

EnforcementState ForcedPreferenceSource::enforcement() const
{
    return EnforcementState::Enforced;
}

int ForcedPreferenceSource::priority() const
{
    return _priority;
}

} // namespace OCC
