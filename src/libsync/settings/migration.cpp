/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>

#include "migration.h"
#include "theme.h"
#include "configfile.h"
#include "version.h"
#include "common/utility.h"

#include <QSettings>
#include <QDir>
#include <QStandardPaths>

namespace {
    constexpr auto accountsC = "Accounts";
    constexpr auto legacyCfgFileNameC = "owncloud.cfg";
    constexpr auto legacyRelativeConfigLocationC = "/ownCloud/owncloud.cfg";
    constexpr auto unbrandedRelativeConfigLocationC = "/Nextcloud/nextcloud.cfg";
    constexpr auto unbrandedCfgFileNameC = "nextcloud.cfg";
}

namespace OCC {

Q_LOGGING_CATEGORY(lcMigration, "nextcloud.settings.migration", QtInfoMsg)

Migration::MigrationPhase Migration::_migrationPhase = MigrationPhase::NotStarted;;
Migration::MigrationType Migration::_migrationType = MigrationType::UnbrandedToUnbranded;
Migration::VersionChangeType Migration::_versionChangeType = VersionChangeType::NoVersionChange;
QString Migration::_discoveredLegacyConfigPath = {};
Migration::LegacyData Migration::_configSettings = {};

QVersionNumber Migration::currentVersion() const
{
    return QVersionNumber::fromString(MIRALL_VERSION_STRING);
}

QVersionNumber Migration::previousVersion() const
{
    return QVersionNumber::fromString(ConfigFile().clientPreviousVersionString());
}

QVersionNumber Migration::configVersion() const
{
    return QVersionNumber::fromString(ConfigFile().clientVersionString());
}

void Migration::setMigrationPhase(const MigrationPhase phase)
{
    // do not rollback
    if (phase > _migrationPhase) {
        _migrationPhase = phase;
    }
}

Migration::MigrationPhase Migration::migrationPhase() const
{
    return _migrationPhase;
}

void Migration::setMigrationType(const MigrationType type)
{
    _migrationType = type;
}

Migration::MigrationType Migration::migrationType() const
{
    return _migrationType;
}

Migration::VersionChangeType Migration::versionChangeType() const
{
    return _versionChangeType;
}

void Migration::setVersionChangeType(const VersionChangeType type)
{
    _versionChangeType = type;
}

bool Migration::isUpgrade()
{
    const auto isUpgrade = currentVersion() > previousVersion();
    if (isUpgrade) {
        setVersionChangeType(VersionChangeType::Upgrade);
    }
    return versionChangeType() == VersionChangeType::Upgrade;
}

bool Migration::isDowngrade()
{
    const auto isDowngrade = previousVersion() > currentVersion();
    if (isDowngrade) {
        setVersionChangeType(VersionChangeType::Downgrade);
    }
    return versionChangeType() == VersionChangeType::Downgrade;
}

bool Migration::versionChanged()
{
    return isUpgrade() || isDowngrade();
}

bool Migration::shouldTryUnbrandedToBrandedMigration() const
{
    const auto isUnbrandedToBranded = migrationPhase() == Migration::MigrationPhase::SetupFolders
        && Theme::instance()->appName() != ConfigFile::unbrandedAppName 
        && !_discoveredLegacyConfigPath.isEmpty();

    if (isUnbrandedToBranded) {
        Migration().setMigrationType(MigrationType::UnbrandedToBranded);
    }
    return migrationType() == MigrationType::UnbrandedToBranded;
}

bool Migration::isUnbrandedToBrandedMigration() const
{
    return isInProgress() && migrationType() == MigrationType::UnbrandedToBranded;
}

bool Migration::shouldTryToMigrate()
{
    return !isClientVersionSet() && (isUpgrade() || isDowngrade());
}

bool Migration::isClientVersionSet() const
{
    const auto configVersionNumber = configVersion();
    const auto previousVersionNumber = previousVersion();
    return !configVersionNumber.isNull() && !previousVersionNumber.isNull()
        && configVersionNumber == previousVersionNumber;
}

bool Migration::isInProgress() const
{
    const auto currentPhase = migrationPhase();
    return currentPhase != MigrationPhase::NotStarted
        && currentPhase != MigrationPhase::Done;
}

Migration::LegacyData Migration::legacyData() const
{
    qCInfo(lcMigration) << "Migrate: restoreFromLegacySettings, checking settings group" << Theme::instance()->appName();

    // try to open the correctly themed settings
    auto settings = ConfigFile::settingsWithGroup(Theme::instance()->appName());
    LegacyData legacyData;

    // if the settings file could not be opened, the childKeys list is empty
    // then try to load settings from a very old place
    if (settings->childKeys().isEmpty()) {
        // Legacy settings used QDesktopServices to get the location for the config folder in 2.4 and before
        const auto legacy2_4CfgSettingsLocation = QString(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/data"));
        const auto legacy2_4CfgFileParentFolder = legacy2_4CfgSettingsLocation.left(legacy2_4CfgSettingsLocation.lastIndexOf('/'));

        // 2.5+ (rest of 2.x series)
        const auto legacy2_5CfgSettingsLocation =
            QStandardPaths::writableLocation(Utility::isWindows() ? QStandardPaths::AppDataLocation : QStandardPaths::AppConfigLocation);
        const auto legacy2_5CfgFileParentFolder = legacy2_5CfgSettingsLocation.left(legacy2_5CfgSettingsLocation.lastIndexOf('/'));

        // Now try the locations we use today
        const auto fullLegacyCfgFile = QDir::fromNativeSeparators(settings->fileName());
        const auto legacyCfgFileParentFolder = fullLegacyCfgFile.left(fullLegacyCfgFile.lastIndexOf('/'));
        const auto legacyCfgFileGrandParentFolder = legacyCfgFileParentFolder.left(legacyCfgFileParentFolder.lastIndexOf('/'));

        const auto legacyCfgFileNamePath = QString(QStringLiteral("/") + legacyCfgFileNameC);
        const auto legacyCfgFileRelativePath = QString(legacyRelativeConfigLocationC);

        auto legacyLocations = QVector<QString>{legacy2_4CfgFileParentFolder + legacyCfgFileRelativePath,
                                                legacy2_5CfgFileParentFolder + legacyCfgFileRelativePath,
                                                legacyCfgFileParentFolder + legacyCfgFileNamePath,
                                                legacyCfgFileGrandParentFolder + legacyCfgFileRelativePath};

        if (Theme::instance()->isBranded()) {
            const auto unbrandedCfgFileNamePath = QString(QStringLiteral("/") + unbrandedCfgFileNameC);
            const auto unbrandedCfgFileRelativePath = QString(unbrandedRelativeConfigLocationC);
            legacyLocations.append({legacyCfgFileParentFolder + unbrandedCfgFileNamePath, legacyCfgFileGrandParentFolder + unbrandedCfgFileRelativePath});
        }

        for (const auto &configFileString : std::as_const(legacyLocations)) {
            auto oCSettings = std::make_unique<QSettings>(configFileString, QSettings::IniFormat);
            if (oCSettings->status() != QSettings::Status::NoError) {
                qCInfo(lcMigration) << "Error reading legacy configuration file" << oCSettings->status();
                break;
            }

            if (const QFileInfo configFileInfo(configFileString); configFileInfo.exists() && configFileInfo.isReadable()) {
                ConfigFile configFile;
                const auto legacyVersion = oCSettings->value(ConfigFile::clientVersionC, {}).toString();
                configFile.setClientPreviousVersionString(legacyVersion);
                qCInfo(lcMigration) << "Migrating from" << legacyVersion;
                qCInfo(lcMigration) << "Copy settings" << oCSettings->allKeys().join(", ");
                Migration migration;
                migration.setDiscoveredLegacyConfigPath(configFileInfo.canonicalPath());
                legacyData.configFile = configFileString;
                legacyData.settings.create(oCSettings.release());
                migration.setLegacyData(legacyData);
                break;
            } else {
                qCInfo(lcMigration) << "Migrate: could not read old config " << configFileString;
            }
        }
    }

    return legacyData;
}

QString Migration::discoveredLegacyConfigPath() const
{
    return _discoveredLegacyConfigPath;
}

void Migration::setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath)
{
    if (_discoveredLegacyConfigPath == discoveredLegacyConfigPath) {
        return;
    }

    _discoveredLegacyConfigPath = discoveredLegacyConfigPath;
}

void Migration::setLegacyData(const LegacyData &LegacyData)
{
    _configSettings.configFile = LegacyData.configFile;
    _configSettings.settings = LegacyData.settings;
}

}
