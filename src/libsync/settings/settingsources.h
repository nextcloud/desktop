/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SETTINGSOURCES_H
#define SETTINGSOURCES_H

#include <QString>

#include "owncloudlib.h"
#include "settings/managedsettings.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT UserConfigSource : public SettingSource
{
public:
    explicit UserConfigSource(QString configFilePath);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceKind kind() const override;
    [[nodiscard]] LockState lockState() const override;
    [[nodiscard]] int priority() const override;

private:
    QString _configFilePath;
};

// Reads a native OS store (Windows registry, macOS plist, Linux conf) at a fixed location.
class OWNCLOUDSYNC_EXPORT NativeSettingsSource : public SettingSource
{
public:
    NativeSettingsSource(QString location, SettingSourceKind kind, LockState lockState, int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceKind kind() const override;
    [[nodiscard]] LockState lockState() const override;
    [[nodiscard]] int priority() const override;

private:
    QString _location;
    SettingSourceKind _kind;
    LockState _lockState;
    int _priority;
};

// Ordered device sources for the running platform, using the same app name
// selection as ConfigFile::getValue and getPolicySetting.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildDeviceSources();

} // namespace OCC

#endif // SETTINGSOURCES_H
