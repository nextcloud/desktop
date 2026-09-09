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
    // A non empty group is always used and overrides the group passed to read().
    explicit UserConfigSource(QString configFilePath, QString group = {});

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceType type() const override;
    [[nodiscard]] EnforcementState enforcement() const override;
    [[nodiscard]] int priority() const override;

private:
    QString _configFilePath;
    QString _group;
};

// Reads a native OS store (Windows registry, macOS plist, Linux conf) at a fixed location.
class OWNCLOUDSYNC_EXPORT NativeSettingsSource : public SettingSource
{
public:
    NativeSettingsSource(QString location, SettingSourceType kind, EnforcementState enforcement, int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceType type() const override;
    [[nodiscard]] EnforcementState enforcement() const override;
    [[nodiscard]] int priority() const override;

private:
    QString _location;
    SettingSourceType _kind;
    EnforcementState _enforcement;
    int _priority;
};

// Base for an enforced policy source that contributes a value only when an
// administrator has forced the key.
class OWNCLOUDSYNC_EXPORT ForcedPreferenceSource : public SettingSource
{
public:
    explicit ForcedPreferenceSource(int priority);

    [[nodiscard]] std::optional<QVariant> read(const QString &key, const QString &group) const override;
    [[nodiscard]] SettingSourceType type() const override;
    [[nodiscard]] EnforcementState enforcement() const override;
    [[nodiscard]] int priority() const override;

protected:
    [[nodiscard]] virtual bool isForced(const QString &key) const = 0;
    // std::nullopt means the key has no value in the domain.
    [[nodiscard]] virtual std::optional<QVariant> copyForcedValue(const QString &key) const = 0;

private:
    int _priority;
};

#ifdef Q_OS_MACOS
// Reads macOS managed preferences for an application domain, treating a key as
// enforced only when CFPreferencesAppValueIsForced reports it forced.
class OWNCLOUDSYNC_EXPORT MacForcedPreferenceSource : public ForcedPreferenceSource
{
public:
    MacForcedPreferenceSource(QString applicationId, int priority);

protected:
    [[nodiscard]] bool isForced(const QString &key) const override;
    [[nodiscard]] std::optional<QVariant> copyForcedValue(const QString &key) const override;

private:
    QString _applicationId;
};
#endif

// Ordered device sources for the running platform.
[[nodiscard]] OWNCLOUDSYNC_EXPORT std::vector<std::unique_ptr<SettingSource>> buildDeviceSources();

} // namespace OCC

#endif // SETTINGSOURCES_H
