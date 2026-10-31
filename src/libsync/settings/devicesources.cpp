/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/devicesources.h"

#include "config.h"
#include "configfile.h"
#include "settings/migration.h"
#include "settings/settingpriorities.h"
#include "settings/settingsources.h"
#include "theme.h"

namespace OCC
{

std::vector<std::unique_ptr<SettingSource>> buildDeviceSources()
{
    [[maybe_unused]] const auto app =
        Migration::isUnbrandedToBrandedMigration() ? QString::fromLatin1(ConfigFile::unbrandedAppName) : Theme::instance()->appNameGUI();

    std::vector<std::unique_ptr<SettingSource>> sources;
#if defined(Q_OS_WIN)
    sources.push_back(
        std::make_unique<NativeSettingsSource>(QStringLiteral(R"(HKEY_CURRENT_USER\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
                                               SettingSourceType::PlatformPolicy,
                                               EnforcementState::Enforced,
                                               SettingPriority::userPolicy));
    sources.push_back(std::make_unique<NativeSettingsSource>(
        QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\Policies\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
        SettingSourceType::PlatformPolicy,
        EnforcementState::Enforced,
        SettingPriority::machinePolicy));
    sources.push_back(
        std::make_unique<NativeSettingsSource>(QStringLiteral(R"(HKEY_LOCAL_MACHINE\Software\%1\%2)").arg(QString::fromLatin1(APPLICATION_VENDOR), app),
                                               SettingSourceType::PlatformDefault,
                                               EnforcementState::NotEnforced,
                                               SettingPriority::deviceDefault));
#elif defined(Q_OS_MACOS)
    // CFPreferences honors both host and per user managed preferences.
    sources.push_back(std::make_unique<MacForcedPreferenceSource>(QStringLiteral(APPLICATION_REV_DOMAIN), SettingPriority::machinePolicy));
    sources.push_back(std::make_unique<NativeSettingsSource>(QStringLiteral("/Library/Preferences/" APPLICATION_REV_DOMAIN ".plist"),
                                                             SettingSourceType::PlatformDefault,
                                                             EnforcementState::NotEnforced,
                                                             SettingPriority::deviceDefault));
#else
    sources.push_back(std::make_unique<NativeSettingsSource>(QStringLiteral(SYSCONFDIR "/%1/policies.conf").arg(app),
                                                             SettingSourceType::PlatformPolicy,
                                                             EnforcementState::Enforced,
                                                             SettingPriority::machinePolicy));
    sources.push_back(std::make_unique<NativeSettingsSource>(QStringLiteral(SYSCONFDIR "/%1/%1.conf").arg(app),
                                                             SettingSourceType::PlatformDefault,
                                                             EnforcementState::NotEnforced,
                                                             SettingPriority::deviceDefault));
#endif
    return sources;
}

} // namespace OCC
