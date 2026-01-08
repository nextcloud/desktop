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

Migration::Phase Migration::_phase = Phase::NotStarted;;
Migration::BrandingType Migration::_brandingType = BrandingType::UnbrandedToUnbranded;
Migration::UpgradeType Migration::_upgradeType = UpgradeType::NoChange;
QString Migration::_discoveredLegacyConfigPath = {};
Migration::LegacyData Migration::_legacyData = {};

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

void Migration::setPhase(const Phase phase)
{
    // do not rollback
   if (phase > _phase) {
        _phase = phase;
    }
}

Migration::Phase Migration::phase() const
{
    return _phase;
}

void Migration::setBrandingType(const BrandingType type)
{
    _brandingType = type;
}

Migration::BrandingType Migration::brandingType() const
{
    return _brandingType;
}

Migration::UpgradeType Migration::upgradeType() const
{
    return _upgradeType;
}

void Migration::setUpgradeType(const UpgradeType type)
{
    _upgradeType = type;
}

bool Migration::isUpgrade()
{
    const auto isUpgrade = currentVersion() > previousVersion();
    if (isUpgrade) {
        setUpgradeType(UpgradeType::Upgrade);
    }
    return _upgradeType == UpgradeType::Upgrade;
}

bool Migration::isDowngrade()
{
    const auto isDowngrade = previousVersion() > currentVersion();
    if (isDowngrade) {
        setUpgradeType(UpgradeType::Downgrade);
    }
    return _upgradeType == UpgradeType::Downgrade;
}

bool Migration::versionChanged()
{
    return isUpgrade() || isDowngrade();
}

bool Migration::shouldTryUnbrandedToBrandedMigration()
{
    const auto isUnbrandedToBranded = phase() == Migration::Phase::SetupFolders
        && Theme::instance()->appName() != ConfigFile::unbrandedAppName 
        && !_discoveredLegacyConfigPath.isEmpty();

    if (isUnbrandedToBranded) {
        setBrandingType(BrandingType::UnbrandedToBranded);
    }
    return _brandingType == BrandingType::UnbrandedToBranded;
}

bool Migration::isUnbrandedToBrandedMigration() const
{
    return isInProgress() && brandingType() == BrandingType::UnbrandedToBranded;
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
    const auto currentPhase = phase();
    return currentPhase != Phase::NotStarted 
        && currentPhase != Phase::Done;
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
                legacyData.reset(oCSettings.get());
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

void Migration::setLegacyData(const LegacyData legacyData)
{
    _legacyData = legacyData;
}

}
