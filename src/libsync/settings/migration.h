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
    Migration() { };

    enum Phase {
        NotStarted,
        SetupConfigFile,
        SetupUsers,
        SetupFolders,
        Done
    };

    enum BrandingType {
        UnbrandedToUnbranded,
        UnbrandedToBranded,
        LegacyToUnbranded,
        LegacyToBranded
    };

    enum UpgradeType {
        NoChange,
        Upgrade,
        Downgrade
    };

    using LegacyData = QSharedPointer<QSettings>;

    [[nodiscard]] QVersionNumber previousVersion() const;
    [[nodiscard]] QVersionNumber currentVersion() const;
    [[nodiscard]] QVersionNumber configVersion() const;

    [[nodiscard]] Phase phase() const;
    void setPhase(const Phase phase);

    [[nodiscard]] BrandingType brandingType() const;
    void setBrandingType(const BrandingType type);

    [[nodiscard]] UpgradeType upgradeType() const;
    void setUpgradeType(const UpgradeType type);

    [[nodiscard]] LegacyData legacyData() const;
    void setLegacyData(const LegacyData legacyData);

    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] QString discoveredLegacyConfigPath() const;
    void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);
   
    [[nodiscard]] bool isUpgrade();
    [[nodiscard]] bool isDowngrade();
    [[nodiscard]] bool versionChanged();
    [[nodiscard]] bool shouldTryUnbrandedToBrandedMigration();
    [[nodiscard]] bool isUnbrandedToBrandedMigration() const;
    [[nodiscard]] bool shouldTryToMigrate();
    [[nodiscard]] bool isClientVersionSet() const;
    [[nodiscard]] bool isInProgress() const;

private:
    static Phase _phase;
    static BrandingType _brandingType;
    static UpgradeType _upgradeType;
    static QString _discoveredLegacyConfigPath;
    static LegacyData _legacyData;
};
}
#endif // MIGRATION_H
