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

UserConfigSource::UserConfigSource(QString configFilePath)
    : _configFilePath(std::move(configFilePath))
{
}

std::optional<QVariant> UserConfigSource::read(const QString &key, const QString &group) const
{
    QSettings settings(_configFilePath, QSettings::IniFormat);
    if (!group.isEmpty()) {
        settings.beginGroup(group);
    }
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    return settings.value(key);
}

SettingSourceKind UserConfigSource::kind() const
{
    return SettingSourceKind::UserConfig;
}

LockState UserConfigSource::lockState() const
{
    return LockState::Unlocked;
}

int UserConfigSource::priority() const
{
    return 50;
}

NativeSettingsSource::NativeSettingsSource(QString location, SettingSourceKind kind, LockState lockState, int priority)
    : _location(std::move(location))
    , _kind(kind)
    , _lockState(lockState)
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

SettingSourceKind NativeSettingsSource::kind() const
{
    return _kind;
}

LockState NativeSettingsSource::lockState() const
{
    return _lockState;
}

int NativeSettingsSource::priority() const
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
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(R"(HKEY_CURRENT_USER\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
        SettingSourceKind::PlatformPolicy, LockState::Locked, 210));
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
        SettingSourceKind::PlatformPolicy, LockState::Locked, 200));
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
        SettingSourceKind::PlatformDefault, LockState::Unlocked, 20));
#elif defined(Q_OS_MAC)
    // TODO(mdm): confirm the forced managed preferences API before treating a managed plist as locked.
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral("/Library/Managed Preferences/" APPLICATION_REV_DOMAIN ".plist"),
        SettingSourceKind::PlatformPolicy, LockState::Locked, 200));
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"),
        SettingSourceKind::PlatformDefault, LockState::Unlocked, 20));
#else
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(SYSCONFDIR "/%1/%1.conf").arg(app),
        SettingSourceKind::PlatformDefault, LockState::Unlocked, 20));
#endif
    return sources;
}

} // namespace OCC
