/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QLoggingCategory>

#include "migration.h"
#include "theme.h"
#include "configfile.h"
#include "version.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMigration, "nextcloud.settings.migration", QtInfoMsg)

Migration::Migration()
{
    _migrationPhase = MigrationPhase::NotStarted;
    _migrationType = MigrationType::UnbrandedToUnbranded;
    _versionChangeType = VersionChangeType::NoVersionChange;
}

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
        && Theme::instance()->appName() != ConfigFile::unbrandedAppName;
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

}
