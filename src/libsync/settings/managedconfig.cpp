/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settings/managedconfig.h"
#include "settings/servermanagedsettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace OCC {

namespace {
constexpr char serverManagedSettingsKey[] = "serverManagedSettings";
}

ManagedConfig &ManagedConfig::instance()
{
    static ManagedConfig instance;
    return instance;
}

ServerManagedSettings ManagedConfig::serverSettings(const QString &configFilePath)
{
    {
        QReadLocker locker(&_lock);
        if (_loaded && _configFilePath == configFilePath) {
            return _cached;
        }
    }

    QWriteLocker locker(&_lock);
    if (!_loaded || _configFilePath != configFilePath) {
        _cached = parse(configFilePath);
        _configFilePath = configFilePath;
        _loaded = true;
    }
    return _cached;
}

void ManagedConfig::setServerSettings(const QString &configFilePath, const ServerManagedSettings &settings)
{
    QWriteLocker locker(&_lock);
    save(configFilePath, settings);
    _cached = settings;
    _configFilePath = configFilePath;
    _loaded = true;
}

void ManagedConfig::invalidate()
{
    QWriteLocker locker(&_lock);
    _loaded = false;
}

ServerManagedSettings ManagedConfig::parse(const QString &configFilePath)
{
    QSettings settings(configFilePath, QSettings::IniFormat);
    const auto root = QJsonDocument::fromJson(settings.value(QLatin1String(serverManagedSettingsKey)).toString().toUtf8()).object();

    ServerManagedSettings managed;
    managed.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt();
    managed.defaults = root.value(QStringLiteral("defaults")).toObject().toVariantMap();
    managed.enforced = root.value(QStringLiteral("enforced")).toObject().toVariantMap();
    // Re sanitize the cache: a stale or edited file must not restore keys or enforced values the live path rejects.
    return sanitizeServerManagedSettings(managed);
}

void ManagedConfig::save(const QString &configFilePath, const ServerManagedSettings &settings)
{
    QJsonObject root;
    root[QStringLiteral("schemaVersion")] = settings.schemaVersion;
    root[QStringLiteral("defaults")] = QJsonObject::fromVariantMap(settings.defaults);
    root[QStringLiteral("enforced")] = QJsonObject::fromVariantMap(settings.enforced);

    QSettings iniSettings(configFilePath, QSettings::IniFormat);
    iniSettings.setValue(QLatin1String(serverManagedSettingsKey),
        QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
}

} // namespace OCC
