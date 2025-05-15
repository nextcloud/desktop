#ifndef MIGRATION_H
#define MIGRATION_H

#include <QString>

namespace OCC {

class Migration
{
public:
    Migration();
    /**
     * Looks for config files with different names from older client versions
     * in different locations
     *
     * Returns the found config file path found.
     */
    void findLegacyClientConfigFile();

    /**
     * Maybe a newer version of the client was used with this config file: if so, backup.
     * Return backup files list.
     */
    [[nodiscard]] QStringList backupConfigFiles() const;
    [[nodiscard]] bool isUpgrade() const;
    [[nodiscard]] bool isDowngrade() const;
    [[nodiscard]] QString configFileToRestore() const;
    [[nodiscard]] QString findLegacyConfigFile() const;
    [[nodiscard]] bool makeConfigSettingsBackwardCompatible() const;

    [[nodiscard]] QString backup(const QString &fileName) const;

    /**
     * Returns the list of settings keys that can't be read because
     * they are from the future.
     */
    void accountbackwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys);

    /**
     * Returns a list of keys that can't be read because they are from
     * future versions.
     */
    void folderbackwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys);


    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] QString discoveredLegacyConfigPath() const;
    void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);
    [[nodiscard]] QString discoveredLegacyConfigFile() const;
    void setDiscoveredLegacyConfigFile(const QString &discoveredLegacyConfigFile);

private:
    QString _confDir;
    QString _discoveredLegacyConfigPath;
    QString _discoveredLegacyConfigFile;

};
}
#endif // MIGRATION_H
