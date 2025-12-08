/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIGRATION_H
#define MIGRATION_H

#include <QVersionNumber>
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

    [[nodiscard]] QVersionNumber previousVersion() const;
    [[nodiscard]] QVersionNumber currentVersion() const;
    [[nodiscard]] QVersionNumber configVersion() const;

    [[nodiscard]] MigrationPhase migrationPhase() const;
    [[nodiscard]] MigrationType migrationType() const;
    [[nodiscard]] VersionChangeType versionChangeType() const;

    void setMigrationPhase(const MigrationPhase phase);
    void setMigrationType(const MigrationType type);
    void setVersionChangeType(const VersionChangeType type);

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
};
}
#endif // MIGRATION_H
