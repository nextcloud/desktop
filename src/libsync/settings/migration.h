/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MIGRATION_H
#define MIGRATION_H

#include <QVersionNumber>
#include <QSettings>
#include <QMap>
#include <memory>
#include "owncloudlib.h"

namespace OCC {

class OWNCLOUDSYNC_EXPORT Migration
{
    Q_GADGET
public:
    // All state is process global; this class is a collection of static
    // helpers and is not meant to be instantiated.
    Migration() = delete;

    enum class Phase {
        NotStarted,
        SetupConfigFile,
        SetupUsers,
        SetupFolders,
        Done
    };
    Q_ENUM(Phase)

    enum class BrandingType {
        UnbrandedToUnbranded,
        UnbrandedToBranded,
        LegacyToUnbranded,
        LegacyToBranded
    };
    Q_ENUM(BrandingType)

    enum class UpgradeType {
        NoChange,
        Upgrade,
        Downgrade
    };
    Q_ENUM(UpgradeType)

    using LegacyData = std::unique_ptr<QSettings>;

    [[nodiscard]] static QVersionNumber currentVersion();
    [[nodiscard]] static QVersionNumber configVersion();

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
    [[nodiscard]] static Phase phase();
    static void setPhase(const Phase phase);

    [[nodiscard]] static BrandingType brandingType();
    static void setBrandingType(const BrandingType type);

    [[nodiscard]] static UpgradeType upgradeType();
    static void setUpgradeType(const UpgradeType type);

    /// Returns QSettings from a legacy config file. Ownership is transferred to the caller.
    [[nodiscard]] static LegacyData legacyData();

    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] static QString discoveredLegacyConfigPath();
    static void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);

    [[nodiscard]] static bool isUpgrade();
    [[nodiscard]] static bool isDowngrade();
    [[nodiscard]] static bool versionChanged();
    [[nodiscard]] static bool shouldTryUnbrandedToBrandedMigration();
    [[nodiscard]] static bool isUnbrandedToBrandedMigration();
    [[nodiscard]] static bool shouldTryToMigrate();
    [[nodiscard]] static bool isInProgress();

    /// Resets all shared state to initial values. Only intended for use in unit tests.
    static void resetForTesting();

private:
    static Phase _phase;
    static BrandingType _brandingType;
    static UpgradeType _upgradeType;
    static QString _discoveredLegacyConfigPath;
};
}
#endif // MIGRATION_H
