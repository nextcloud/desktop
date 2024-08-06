/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "owncloudlib.h"
#include <memory>
#include <QSharedPointer>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <chrono>

class QWidget;
class QHeaderView;
class ExcludedFiles;

namespace OCC {

class AbstractCredentials;

/**
 * @brief The ConfigFile class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT ConfigFile
{
public:
    ConfigFile();

    enum Scope { UserScope,
        SystemScope, LegacyScope };

    [[nodiscard]] QString configPath() const;
    [[nodiscard]] QString configFile() const;
    [[nodiscard]] QString excludeFile(Scope scope) const;
    static QString excludeFileFromSystem(); // doesn't access config dir

    /**
     * Creates a backup of any given fileName in the config folder
     *
     * Returns the path of the new backup.
     */
    [[nodiscard]] QString backup(const QString &fileName) const;
    /**
     * Display warning with a list of the config files that were backed up
     */
    [[nodiscard]] bool showConfigBackupWarning() const;

    bool exists();

    [[nodiscard]] QString defaultConnection() const;

    // the certs do not depend on a connection.
    QByteArray caCerts();
    void setCaCerts(const QByteArray &);

    bool passwordStorageAllowed(const QString &connection = QString());

    /* Server poll interval in milliseconds */
    [[nodiscard]] std::chrono::milliseconds remotePollInterval(const QString &connection = QString()) const;
    /* Set poll interval. Value in milliseconds has to be larger than 5000 */
    void setRemotePollInterval(std::chrono::milliseconds interval, const QString &connection = QString());

    /* Interval to check for new notifications */
    [[nodiscard]] std::chrono::milliseconds notificationRefreshInterval(const QString &connection = QString()) const;

    /* Force sync interval, in milliseconds */
    [[nodiscard]] std::chrono::milliseconds forceSyncInterval(const QString &connection = QString()) const;

    /**
     * Interval in milliseconds within which full local discovery is required
     *
     * Use -1 to disable regular full local discoveries.
     */
    [[nodiscard]] std::chrono::milliseconds fullLocalDiscoveryInterval() const;

    [[nodiscard]] bool monoIcons() const;
    void setMonoIcons(bool);

    [[nodiscard]] bool promptDeleteFiles() const;
    void setPromptDeleteFiles(bool promptDeleteFiles);

    [[nodiscard]] bool crashReporter() const;
    void setCrashReporter(bool enabled);

    [[nodiscard]] bool automaticLogDir() const;
    void setAutomaticLogDir(bool enabled);

    [[nodiscard]] QString logDir() const;
    void setLogDir(const QString &dir);

    [[nodiscard]] bool logDebug() const;
    void setLogDebug(bool enabled);

    [[nodiscard]] int logExpire() const;
    void setLogExpire(int hours);

    [[nodiscard]] bool logFlush() const;
    void setLogFlush(bool enabled);

    // Whether experimental UI options should be shown
    [[nodiscard]] bool showExperimentalOptions() const;

    // proxy settings
    void setProxyType(int proxyType,
        const QString &host = QString(),
        int port = 0, bool needsAuth = false,
        const QString &user = QString(),
        const QString &pass = QString());

    [[nodiscard]] int proxyType() const;
    [[nodiscard]] QString proxyHostName() const;
    [[nodiscard]] int proxyPort() const;
    [[nodiscard]] bool proxyNeedsAuth() const;
    [[nodiscard]] QString proxyUser() const;
    [[nodiscard]] QString proxyPassword() const;

    /** 0: no limit, 1: manual, >0: automatic */
    [[nodiscard]] int useUploadLimit() const;
    [[nodiscard]] int useDownloadLimit() const;
    void setUseUploadLimit(int);
    void setUseDownloadLimit(int);
    /** in kbyte/s */
    [[nodiscard]] int uploadLimit() const;
    [[nodiscard]] int downloadLimit() const;
    void setUploadLimit(int kbytes);
    void setDownloadLimit(int kbytes);
    /** [checked, size in MB] **/
    [[nodiscard]] QPair<bool, qint64> newBigFolderSizeLimit() const;
    void setNewBigFolderSizeLimit(bool isChecked, qint64 mbytes);
    [[nodiscard]] bool notifyExistingFoldersOverLimit() const;
    void setNotifyExistingFoldersOverLimit(const bool notify);
    [[nodiscard]] bool stopSyncingExistingFoldersOverLimit() const;
    void setStopSyncingExistingFoldersOverLimit(const bool stopSyncing);
    [[nodiscard]] bool useNewBigFolderSizeLimit() const;
    [[nodiscard]] bool confirmExternalStorage() const;
    void setConfirmExternalStorage(bool);

    /** If we should move the files deleted on the server in the trash  */
    [[nodiscard]] bool moveToTrash() const;
    void setMoveToTrash(bool);

    /** If we should force loginflow v2 */
    [[nodiscard]] bool forceLoginV2() const;
    void setForceLoginV2(bool);

    [[nodiscard]] bool showMainDialogAsNormalWindow() const;

    static bool setConfDir(const QString &value);

    [[nodiscard]] bool optionalServerNotifications() const;
    void setOptionalServerNotifications(bool show);

    [[nodiscard]] bool showCallNotifications() const;
    void setShowCallNotifications(bool show);

    [[nodiscard]] bool showInExplorerNavigationPane() const;
    void setShowInExplorerNavigationPane(bool show);

    [[nodiscard]] int timeout() const;
    [[nodiscard]] qint64 chunkSize() const;
    [[nodiscard]] qint64 maxChunkSize() const;
    [[nodiscard]] qint64 minChunkSize() const;
    [[nodiscard]] std::chrono::milliseconds targetChunkUploadDuration() const;

    void saveGeometry(QWidget *w);
    void restoreGeometry(QWidget *w);

    // how often the check about new versions runs
    [[nodiscard]] std::chrono::milliseconds updateCheckInterval(const QString &connection = QString()) const;

    // skipUpdateCheck completely disables the updater and hides its UI
    [[nodiscard]] bool skipUpdateCheck(const QString &connection = QString()) const;
    void setSkipUpdateCheck(bool, const QString &);

    // autoUpdateCheck allows the user to make the choice in the UI
    [[nodiscard]] bool autoUpdateCheck(const QString &connection = QString()) const;
    void setAutoUpdateCheck(bool, const QString &);

    /** Query-parameter 'updatesegment' for the update check, value between 0 and 99.
        Used to throttle down desktop release rollout in order to keep the update servers alive at peak times.
        See: https://github.com/nextcloud/client_updater_server/pull/36 */
    [[nodiscard]] int updateSegment() const;

    [[nodiscard]] QString currentUpdateChannel() const;
    [[nodiscard]] QString defaultUpdateChannel() const;
    [[nodiscard]] QStringList validUpdateChannels() const;
    void setUpdateChannel(const QString &channel);

    [[nodiscard]] QString overrideServerUrl() const;
    void setOverrideServerUrl(const QString &url);

    [[nodiscard]] QString overrideLocalDir() const;
    void setOverrideLocalDir(const QString &localDir);

    [[nodiscard]] bool isVfsEnabled() const;
    void setVfsEnabled(bool enabled);

    void saveGeometryHeader(QHeaderView *header);
    void restoreGeometryHeader(QHeaderView *header);

    [[nodiscard]] QString certificatePath() const;
    void setCertificatePath(const QString &cPath);
    [[nodiscard]] QString certificatePasswd() const;
    void setCertificatePasswd(const QString &cPasswd);

    /** The client version that last used this settings file.
        Updated by configVersionMigration() at client startup. */
    [[nodiscard]] QString clientVersionString() const;
    void setClientVersionString(const QString &version);

    /** If the option 'Launch on system startup' is set
        Updated by configVersionMigration() at client startup. */
    [[nodiscard]] bool launchOnSystemStartup() const;
    void setLaunchOnSystemStartup(const bool autostart);

    [[nodiscard]] bool serverHasValidSubscription() const;
    void setServerHasValidSubscription(bool valid);

    [[nodiscard]] QString desktopEnterpriseChannel() const;
    void setDesktopEnterpriseChannel(const QString &channel);

    /**  Returns a new settings pre-set in a specific group.  The Settings will be created
         with the given parent. If no parent is specified, the caller must destroy the settings */
    static std::unique_ptr<QSettings> settingsWithGroup(const QString &group, QObject *parent = nullptr);

    /// Add the system and user exclude file path to the ExcludedFiles instance.
    static void setupDefaultExcludeFilePaths(ExcludedFiles &excludedFiles);

    /// Set during first time migration of legacy accounts in AccountManager
    [[nodiscard]] static QString discoveredLegacyConfigPath();
    static void setDiscoveredLegacyConfigPath(const QString &discoveredLegacyConfigPath);

protected:
    [[nodiscard]] QVariant getPolicySetting(const QString &policy, const QVariant &defaultValue = QVariant()) const;
    void storeData(const QString &group, const QString &key, const QVariant &value);
    [[nodiscard]] QVariant retrieveData(const QString &group, const QString &key) const;
    void removeData(const QString &group, const QString &key);
    [[nodiscard]] bool dataExists(const QString &group, const QString &key) const;

private:
    [[nodiscard]] QVariant getValue(const QString &param, const QString &group = QString(),
        const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    [[nodiscard]] QString keychainProxyPasswordKey() const;

    using SharedCreds = QSharedPointer<AbstractCredentials>;

    static QString _confDir;
    static QString _discoveredLegacyConfigPath;
};
}
#endif // CONFIGFILE_H
