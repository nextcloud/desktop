/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/settingsources.h"

#include "config.h"
#include "configfile.h"
#include "theme.h"
#include "settings/migration.h"

#include <QSettings>

namespace OCC {

UserConfigSource::UserConfigSource(QString configFilePath, QString group)
    : _configFilePath(std::move(configFilePath))
    , _group(std::move(group))
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
    return 50;
}

NativeSettingsSource::NativeSettingsSource(QString location, SettingSourceType kind, EnforcementState enforcement, int priority)
    : _location(std::move(location))
    , _kind(kind)
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
    return _kind;
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

std::vector<std::unique_ptr<SettingSource>> buildDeviceSources()
{
    [[maybe_unused]] const auto app = Migration::isUnbrandedToBrandedMigration()
        ? QString::fromLatin1(ConfigFile::unbrandedAppName)
        : Theme::instance()->appNameGUI();

    std::vector<std::unique_ptr<SettingSource>> sources;
#if defined(Q_OS_WIN)
    sources.push_back(
        std::make_unique<NativeSettingsSource>(QStringLiteral(R"(HKEY_CURRENT_USER\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
                                               SettingSourceType::PlatformPolicy,
                                               EnforcementState::Enforced,
                                               210));
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
        SettingSourceType::PlatformPolicy,
        EnforcementState::Enforced,
        200));
    sources.push_back(
        std::make_unique<NativeSettingsSource>(QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
                                               SettingSourceType::PlatformDefault,
                                               EnforcementState::NotEnforced,
                                               20));
#elif defined(Q_OS_MACOS)
    // A key counts as enforced only when the MDM profile forces it, resolved through
    // CFPreferences so both host and per user managed preferences are honored.
    sources.push_back(std::make_unique<MacForcedPreferenceSource>(
        QStringLiteral(APPLICATION_REV_DOMAIN), 200));
    sources.push_back(std::make_unique<NativeSettingsSource>(QStringLiteral("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"),
                                                             SettingSourceType::PlatformDefault,
                                                             EnforcementState::NotEnforced,
                                                             20));
#else
    sources.push_back(std::make_unique<NativeSettingsSource>(QStringLiteral(SYSCONFDIR "/%1/%1.conf").arg(app),
                                                             SettingSourceType::PlatformDefault,
                                                             EnforcementState::NotEnforced,
                                                             20));
#endif
    return sources;
}

} // namespace OCC
