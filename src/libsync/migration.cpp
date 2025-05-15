/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "migration.h"

#include "theme.h"
#include "config.h"
#include "configfile.h"
#include "common/utility.h"
#include "version.h"
#include "gui/folder.h"

#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QVersionNumber>

namespace {
constexpr auto legacyRelativeConfigLocationC = "/ownCloud/owncloud.cfg";
constexpr auto legacyCfgFileNameC = "owncloud.cfg";
constexpr auto unbrandedRelativeConfigLocationC = "/Nextcloud/nextcloud.cfg";
constexpr auto unbrandedCfgFileNameC = "nextcloud.cfg";

constexpr auto accountsC = "Accounts";
constexpr auto versionC = "version";

constexpr auto maxAccountsVersion = 13;
constexpr auto maxAccountVersion = 13;

constexpr auto settingsAccountsC = "Accounts";
constexpr auto settingsFoldersC = "Folders";
constexpr auto settingsFoldersWithPlaceholdersC = "FoldersWithPlaceholders";
constexpr auto settingsVersionC = "version";
constexpr auto maxFoldersVersion = 1;
}

namespace OCC {

Q_LOGGING_CATEGORY(lcMigration, "nextcloud.gui.migration", QtInfoMsg);

Migration::Migration()
{}

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

QString Migration::discoveredLegacyConfigFile() const
{
    return _discoveredLegacyConfigFile;
}

void Migration::setDiscoveredLegacyConfigFile(const QString &discoveredLegacyConfigFile)
{
    if (_discoveredLegacyConfigFile == discoveredLegacyConfigFile) {
        return;
    }

    _discoveredLegacyConfigFile = discoveredLegacyConfigFile;
}

void Migration::findLegacyClientConfigFile()
{
    qCInfo(lcMigration) << "Migrate: findLegacyClientConfigFile" << Theme::instance()->appName();

           // Legacy settings used QDesktopServices to get the location for the config folder in 2.4 and before
    const auto legacyStandardPaths = QString(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/data"));
    const auto legacyStandardPathsParentFolder = legacyStandardPaths.left(legacyStandardPaths.lastIndexOf('/'));

           // 2.5+ (rest of 2.x series)
    const auto standardPaths = QStandardPaths::writableLocation(Utility::isWindows() ? QStandardPaths::AppDataLocation : QStandardPaths::AppConfigLocation);
    const auto standardPathsParentFolder = standardPaths.left(standardPaths.lastIndexOf('/'));

           // Now try the locations we use today
    const auto fullLegacyCfgFile = QDir::fromNativeSeparators(ConfigFile().configFile());
    const auto legacyCfgFileParentFolder = fullLegacyCfgFile.left(fullLegacyCfgFile.lastIndexOf('/'));
    const auto legacyCfgFileGrandParentFolder = legacyCfgFileParentFolder.left(legacyCfgFileParentFolder.lastIndexOf('/'));

    const auto legacyCfgFileNamePath = QString(QStringLiteral("/") + legacyCfgFileNameC);
    const auto legacyCfgFileRelativePath = QString(legacyRelativeConfigLocationC);

    auto legacyLocations = QVector<QString>{legacyStandardPathsParentFolder + legacyCfgFileRelativePath,
                                            standardPathsParentFolder + legacyCfgFileRelativePath,
                                            legacyCfgFileGrandParentFolder + legacyCfgFileRelativePath,
                                            legacyCfgFileParentFolder + legacyCfgFileNamePath};

    if (Theme::instance()->isBranded()) {
        const auto unbrandedCfgFileNamePath = QString(QStringLiteral("/") + unbrandedCfgFileNameC);
        const auto unbrandedCfgFileRelativePath = QString(unbrandedRelativeConfigLocationC);
        const auto brandedLegacyCfgFilePath = QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/") + APPLICATION_SHORTNAME + QStringLiteral("/"));
        const auto brandedLegacyCfgFile = QString(APPLICATION_CONFIG_NAME + QStringLiteral(".cfg"));
        legacyLocations.append({brandedLegacyCfgFilePath + brandedLegacyCfgFile,
                                standardPathsParentFolder + unbrandedCfgFileRelativePath,
                                legacyCfgFileParentFolder + unbrandedCfgFileNamePath,
                                legacyCfgFileGrandParentFolder + unbrandedCfgFileRelativePath});
    }

    qCDebug(lcMigration) << "Looking for existing config files at:" << legacyLocations;

    for (const auto &configFile : legacyLocations) {
        auto oCSettings = std::make_unique<QSettings>(configFile, QSettings::IniFormat);
        if (oCSettings->status() != QSettings::Status::NoError) {
            qCInfo(lcMigration) << "Error reading legacy configuration file" << oCSettings->status();
            break;
        }

        const QFileInfo configFileInfo(configFile);
        if (!configFileInfo.exists() || !configFileInfo.isReadable()) {
            qCInfo(lcMigration()) << "Migrate: could not read old config " << configFile;
            continue;
        }

        qCInfo(lcMigration) << "Migrate: old config file" << configFile;
        setDiscoveredLegacyConfigFile(configFileInfo.filePath());
        setDiscoveredLegacyConfigPath(configFileInfo.canonicalPath());
        break;
    }
}

bool Migration::isUpgrade() const
{
    return QVersionNumber::fromString(MIRALL_VERSION_STRING) > QVersionNumber::fromString(ConfigFile().clientVersionString());
}

bool Migration::isDowngrade() const
{
    return QVersionNumber::fromString(ConfigFile().clientVersionString()) > QVersionNumber::fromString(MIRALL_VERSION_STRING);
}

QString Migration::configFileToRestore() const
{
    const auto legacyConfigFile = discoveredLegacyConfigFile();
    if (legacyConfigFile.isEmpty()) {
        return ConfigFile().configFile();
    }

    return legacyConfigFile;
}

QString Migration::backup(const QString &fileName) const
{
    const QString baseFilePath = ConfigFile().configPath() + fileName;
    auto versionString = ConfigFile().clientVersionString();

    if (!versionString.isEmpty()) {
        versionString.prepend('_');
    }

    QString backupFile =
        QStringLiteral("%1.backup_%2%3")
            .arg(baseFilePath)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))
            .arg(versionString);

    // If this exact file already exists it's most likely that a backup was
    // already done. (two backup calls directly after each other, potentially
    // even with source alterations in between!)
    // QFile does not overwrite backupFile
    if(!QFile::copy(baseFilePath, backupFile)) {
        qCWarning(lcMigration) << "Failed to create a backup of the config file" << baseFilePath;
    }

    return backupFile;
}

QStringList Migration::backupConfigFiles() const
{
    // 'Launch on system startup' defaults to true > 3.11.x
    const auto theme = Theme::instance();
    ConfigFile().setLaunchOnSystemStartup(ConfigFile().launchOnSystemStartup());
    Utility::setLaunchOnStartup(theme->appName(), theme->appNameGUI(), ConfigFile().launchOnSystemStartup());

    // default is now off to displaying dialog warning user of too many files deletion
    ConfigFile().setPromptDeleteFiles(false);

    // back up all old config files
    QStringList backupFilesList;
    QDir configDir(ConfigFile().configPath());
    const auto anyConfigFileNameList = configDir.entryInfoList({"*.cfg"}, QDir::Files);
    for (const auto &oldConfig : anyConfigFileNameList) {
        const auto oldConfigFileName = oldConfig.fileName();
        const auto oldConfigFilePath = oldConfig.filePath();
        const auto newConfigFileName = ConfigFile().configFile();
        backupFilesList.append(backup(oldConfigFileName));
        if (oldConfigFilePath != newConfigFileName) {
            if (!QFile::rename(oldConfigFilePath, newConfigFileName)) {
                qCWarning(lcMigration) << "Failed to rename configuration file from" << oldConfigFilePath << "to" << newConfigFileName;
            }
        }
    }

    return backupFilesList;
}

void Migration::accountbackwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    const auto settings = ConfigFile::settingsWithGroup(QLatin1String(accountsC));
    const auto accountsVersion = settings->value(QLatin1String(versionC)).toInt();

    qCInfo(lcMigration) << "Checking for accounts versions.";
    qCInfo(lcMigration) << "Config accounts version:" << accountsVersion;
    qCInfo(lcMigration) << "Max accounts Version is set to:" << maxAccountsVersion;
    if (accountsVersion <= maxAccountsVersion) {
        const auto settingsChildGroups = settings->childGroups();
        for (const auto &accountId : settingsChildGroups) {
            settings->beginGroup(accountId);
            const auto accountVersion = settings->value(QLatin1String(versionC), 1).toInt();

            if (accountVersion > maxAccountVersion) {
                ignoreKeys->append(settings->group());
                qCInfo(lcMigration) << "Ignoring account" << accountId << "because of version" << accountVersion;
            }
            settings->endGroup();
        }
    } else {
        deleteKeys->append(settings->group());
    }
}

void Migration::folderbackwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys)
{
    auto settings = ConfigFile::settingsWithGroup(QLatin1String("Accounts"));

    auto processSubgroup = [&](const QString &name) {
        settings->beginGroup(name);
        const auto foldersVersion = settings->value(QLatin1String(settingsVersionC), 1).toInt();
        qCInfo(lcMigration) << "FolderDefinition::maxSettingsVersion:" << FolderDefinition::maxSettingsVersion();
        if (foldersVersion <= maxFoldersVersion) {
            for (const auto &folderAlias : settings->childGroups()) {
                settings->beginGroup(folderAlias);
                const auto folderVersion = settings->value(QLatin1String(settingsVersionC), 1).toInt();
                if (folderVersion > FolderDefinition::maxSettingsVersion()) {
                    qCInfo(lcMigration) << "Ignoring folder:" << folderAlias << "version:" << folderVersion;
                    ignoreKeys->append(settings->group());
                }
                settings->endGroup();
            }
        } else {
            qCInfo(lcMigration) << "Ignoring group:" << name << "version:" << foldersVersion;
            deleteKeys->append(settings->group());
        }
        settings->endGroup();
    };

    const auto settingsChildGroups = settings->childGroups();
    for (const auto &accountId : settingsChildGroups) {
        settings->beginGroup(accountId);
        processSubgroup("Folders");
        processSubgroup("Multifolders");
        processSubgroup("FoldersWithPlaceholders");
        settings->endGroup();
    }
}

bool Migration::makeConfigSettingsBackwardCompatible() const
{
    ConfigFile configFile;
    const auto didVersionChanged = configFile.isUpgrade() || configFile.isDowngrade();
    if (!didVersionChanged) {
        qCInfo(lcMigration) << "No upgrade or downgrade detected.";
        return true;
    }

    configFile.cleanUpdaterConfiguration();

    QStringList deleteKeys, ignoreKeys;
    // accountbackwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);
    // folderbackwardMigrationSettingsKeys(&deleteKeys, &ignoreKeys);
    if (!didVersionChanged && !(!deleteKeys.isEmpty() || (!ignoreKeys.isEmpty() && didVersionChanged))) {
        qCInfo(lcMigration) << "There are no settings to delete or to ignore. No need to change the current config file.";
        return true;
    }

    const auto isDeleteKeysEmpty = deleteKeys.isEmpty();
    if (const auto backupFilesList = configFile.backupConfigFiles();
        configFile.showConfigBackupWarning()
        && backupFilesList.size() > 0
        /*&& !confirmConfigChangesOrQuitApp(isDeleteKeysEmpty, backupFilesList)*/) {
        return false;
    }

    if (!isDeleteKeysEmpty) {
        auto settings = configFile.settingsWithGroup("foo");
        settings->endGroup();

               // Wipe confusing keys from the future, ignore the others
        for (const auto &badKey : std::as_const(deleteKeys)) {
            settings->remove(badKey);
            qCDebug(lcMigration) << "Migration: removed" << badKey << "key from settings.";
        }
    }

    configFile.setClientVersionString(MIRALL_VERSION_STRING);
    qCDebug(lcMigration) << "Client version changed to" << configFile.clientVersionString();

    return true;
}
}
