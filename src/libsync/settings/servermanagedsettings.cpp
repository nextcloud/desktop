/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/servermanagedsettings.h"

#include "common/vfs.h"

#include <QHash>
#include <QNetworkProxy>

namespace OCC {

namespace {
// Keys the client accepts from the server and whether each is server enforceable.
// Update and proxy keys are default only from the server; only device policy
// enforces them, so a server cannot disable updates or reroute traffic.
struct ServerKeyPolicy {
    bool serverEnforceable = false;
};

// Reject values the client cannot use.
bool isServerKeyValueValid(const QString &key, const QVariant &value)
{
    if (key == QStringLiteral("newBigFolderSizeLimit")) {
        auto ok = false;
        const auto limit = value.toLongLong(&ok);
        return ok && limit >= 0;
    }
    if (key == QStringLiteral("virtualFilesMode")) {
        return static_cast<bool>(Vfs::modeFromString(value.toString()));
    }
    if (key == QStringLiteral("proxyType")) {
        auto ok = false;
        const auto type = value.toInt(&ok);
        return ok && type >= QNetworkProxy::DefaultProxy && type <= QNetworkProxy::HttpProxy;
    }
    if (key == QStringLiteral("proxyPort")) {
        auto ok = false;
        const auto port = value.toInt(&ok);
        return ok && port >= 1 && port <= 65535;
    }
    return true;
}

const QHash<QString, ServerKeyPolicy> &validServerKeys()
{
    static const QHash<QString, ServerKeyPolicy> keys = {
        {QStringLiteral("skipUpdateCheck"), {false}},
        {QStringLiteral("autoUpdateCheck"), {false}},
        {QStringLiteral("virtualFilesMode"), {true}},
        {QStringLiteral("proxyHost"), {false}},
        {QStringLiteral("proxyPort"), {false}},
        {QStringLiteral("proxyType"), {false}},
        {QStringLiteral("newBigFolderSizeLimit"), {true}},
        {QStringLiteral("confirmExternalStorage"), {true}},
        {QStringLiteral("useNewBigFolderSizeLimit"), {true}},
        {QStringLiteral("notifyExistingFoldersOverLimit"), {true}},
        {QStringLiteral("stopSyncingExistingFoldersOverLimit"), {true}},
    };
    return keys;
}
}

ServerManagedSettings parseServerManagedSettings(const QVariantMap &desktopClientCapability)
{
    ServerManagedSettings parsed;
    parsed.schemaVersion = desktopClientCapability.value(QStringLiteral("schemaVersion")).toInt();
    parsed.defaults = desktopClientCapability.value(QStringLiteral("defaults")).toMap();
    parsed.enforced = desktopClientCapability.value(QStringLiteral("enforced")).toMap();
    return parsed;
}

ServerManagedSettings sanitizeServerManagedSettings(const ServerManagedSettings &raw)
{
    ServerManagedSettings serverSettings;
    serverSettings.schemaVersion = raw.schemaVersion;

    const auto &validKeys = validServerKeys();
    for (const auto &[key, value] : raw.defaults.asKeyValueRange()) {
        if (validKeys.contains(key) && isServerKeyValueValid(key, value)) {
            serverSettings.defaults.insert(key, value);
        }
    }
    for (const auto &[key, value] : raw.enforced.asKeyValueRange()) {
        const auto policy = validKeys.constFind(key);
        if (policy != validKeys.cend() && policy->serverEnforceable && isServerKeyValueValid(key, value)) {
            serverSettings.enforced.insert(key, value);
        }
    }
    return serverSettings;
}

ServerSettingsSource::ServerSettingsSource(QVariantMap values, SettingSourceType type, EnforcementState enforcement, int priority)
    : _values(std::move(values))
    , _type(type)
    , _enforcement(enforcement)
    , _priority(priority)
{
}

std::optional<QVariant> ServerSettingsSource::read(const QString &key, const QString &) const
{
    if (!_values.contains(key)) {
        return std::nullopt;
    }
    return _values.value(key);
}

SettingSourceType ServerSettingsSource::type() const
{
    return _type;
}

EnforcementState ServerSettingsSource::enforcement() const
{
    return _enforcement;
}

int ServerSettingsSource::priority() const
{
    return _priority;
}

std::vector<std::unique_ptr<SettingSource>> buildServerSources(const ServerManagedSettings &sanitized)
{
    std::vector<std::unique_ptr<SettingSource>> sources;
    if (!sanitized.enforced.isEmpty()) {
        sources.push_back(std::make_unique<ServerSettingsSource>(sanitized.enforced, SettingSourceType::ServerEnforced, EnforcementState::Enforced, 100));
    }
    if (!sanitized.defaults.isEmpty()) {
        sources.push_back(std::make_unique<ServerSettingsSource>(sanitized.defaults, SettingSourceType::ServerDefault, EnforcementState::NotEnforced, 30));
    }
    return sources;
}

} // namespace OCC
