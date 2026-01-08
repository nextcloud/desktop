/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIGRATION_H
#define MIGRATION_H

#include <QVersionNumber>
#include <QSettings>
#include <QMap>
#include "owncloudlib.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT Migration
{
public:
    Migration();

    enum MigrationPhase {
        NotStarted,
        SetupConfigFile,
        SetupUsers,
        SetupFolders,
        Done
    };

    enum MigrationType {
        UnbrandedToUnbranded,
        UnbrandedToBranded,
        LegacyToUnbranded,
        LegacyToBranded
    };

    enum VersionChangeType {
        NoVersionChange,
        Upgrade,
        Downgrade
    };

    struct LegacyData {
        QString configFile;
        QSharedPointer<QSettings> settings;
    };

    [[nodiscard]] QVersionNumber previousVersion() const;
    [[nodiscard]] QVersionNumber currentVersion() const;
    [[nodiscard]] QVersionNumber configVersion() const;

    [[nodiscard]] MigrationPhase migrationPhase() const;
    void setMigrationPhase(const MigrationPhase phase);

    [[nodiscard]] MigrationType migrationType() const;
    void setMigrationType(const MigrationType type);

    [[nodiscard]] VersionChangeType versionChangeType() const;
    void setVersionChangeType(const VersionChangeType type);

    [[nodiscard]] LegacyData legacyData() const;

    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] QString discoveredLegacyConfigPath() const;
    void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);
   
    [[nodiscard]] bool isUpgrade();
    [[nodiscard]] bool isDowngrade();
    [[nodiscard]] bool versionChanged();
    [[nodiscard]] bool shouldTryUnbrandedToBrandedMigration() const;
    [[nodiscard]] bool isUnbrandedToBrandedMigration() const;
    [[nodiscard]] bool shouldTryToMigrate();
    [[nodiscard]] bool isClientVersionSet() const;
    [[nodiscard]] bool isInProgress() const;

private:
    MigrationPhase _migrationPhase;
    MigrationType _migrationType;
    VersionChangeType _versionChangeType;
    QString _discoveredLegacyConfigPath;
    LegacyData _configSettings;
};
}
#endif // MIGRATION_H
