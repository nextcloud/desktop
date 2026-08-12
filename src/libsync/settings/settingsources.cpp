/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/settingsources.h"

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

} // namespace OCC
