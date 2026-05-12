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

    /**
     * Application::configVersionMigration                     [start]
     *   |-- no migration needed ----------------------> Phase::Done
     *   |
     *   Phase::SetupConfigFile
     *   |   backup config files, remove incompatible keys
     *   |
     * Application::setupAccountsAndFolders
     *   |
     *   Phase::SetupUsers
     *   |
     *   Application::restoreLegacyAccount
     *     |
     *     AccountManager::restore
     *       |-- accounts in current config -----------> load accounts
     *       |
     *       AccountManager::restoreFromLegacySettings
     *         |
     *         Migration::legacyData
     *           |   search legacy config locations
     *           |-- no legacy config found -----------> return
     *           |
     *           store legacy QSettings + config path
     *   |
     *   Phase::SetupFolders
     *   |
     *   FolderMan::setupFolders
     *     |-- legacy path known -> setupFoldersMigration
     *     |
     *     load folder definitions
     *   |
     * AccountState::slotCredentialsFetched            [end]
     *   |
     *   Phase::Done
     */
    [[nodiscard]] Phase phase() const;
    void setPhase(const Phase phase);

    [[nodiscard]] BrandingType brandingType() const;
    void setBrandingType(const BrandingType type);

    [[nodiscard]] UpgradeType upgradeType() const;
    void setUpgradeType(const UpgradeType type);

    /// Returns QSettings from a legacy config file
    [[nodiscard]] LegacyData legacyData();
    void setLegacyData(const LegacyData legacyData);

    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] QString discoveredLegacyConfigPath() const;
    void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);
   
    [[nodiscard]] bool isUpgrade() const;
    [[nodiscard]] bool isDowngrade() const;
    [[nodiscard]] bool versionChanged() const;
    [[nodiscard]] bool shouldTryUnbrandedToBrandedMigration();
    [[nodiscard]] bool isUnbrandedToBrandedMigration() const;
    [[nodiscard]] bool shouldTryToMigrate() const;
    [[nodiscard]] bool isClientVersionSet() const;
    [[nodiscard]] bool isInProgress() const;

    /// Resets all shared state to initial values. Only intended for use in unit tests.
    static void resetForTesting();

private:
    static Phase _phase;
    static BrandingType _brandingType;
    static UpgradeType _upgradeType;
    static QString _discoveredLegacyConfigPath;
    static LegacyData _legacyData;
};
}
#endif // MIGRATION_H
