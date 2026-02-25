/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FOLDERMAN_H
#define FOLDERMAN_H

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QList>

#include "folder.h"
#include "folderwatcher.h"
#ifdef Q_OS_WIN
#include "navigationpanehelper.h"
#endif
#include "syncfileitem.h"

class TestFolderMan;
class TestCfApiShellExtensionsIPC;
class TestShareModel;
class TestFolderStatusModel;
class ShareTestHelper;
class EndToEndTestHelper;
class TestSyncConflictsModel;
class TestRemoteWipe;
class FolderManTestHelper;
class TestFileActionsModel;

namespace OCC {

class Application;
class SyncResult;
class SocketApi;
class LockWatcher;
class UpdateE2eeFolderUsersMetadataJob;

/**
 * @brief The FolderMan class
 * @ingroup gui
 *
 * The FolderMan knows about all loaded folders and is responsible for
 * scheduling them when necessary.
 *
 * A folder is scheduled if:
 * - The configured force-sync-interval has expired
 *   (_timeScheduler and slotScheduleFolderByTime())
 *
 * - A folder watcher receives a notification about a file change
 *   (_folderWatchers and Folder::slotWatchedPathChanged())
 *
 * - The folder etag on the server has changed
 *   (_etagPollTimer)
 *
 * - The locks of a monitored file are released
 *   (_lockWatcher and slotWatchedFileUnlocked())
 *
 * - There was a sync error or a follow-up sync is requested
 *   (_timeScheduler and slotScheduleFolderByTime()
 *    and Folder::slotSyncFinished())
 */
class FolderMan : public QObject
{
    Q_OBJECT
public:
    enum class PathValidityResult {
        Valid,
        ErrorRecursiveValidity,
        ErrorContainsFolder,
        ErrorContainedInFolder,
        ErrorNonEmptyFolder
    };

    enum class GoodPathStrategy {
        AllowOnlyNewPath,
        AllowOverrideExistingPath,
    };

    ~FolderMan() override;
    static FolderMan *instance();

    int setupFolders();
    int setupFoldersMigration();

    /**
     * Returns a list of keys that can't be read because they are from
     * future versions.
     */
    static void backwardMigrationSettingsKeys(QStringList *deleteKeys, QStringList *ignoreKeys);

    [[nodiscard]] const Folder::Map &map() const;

    /** Adds a folder for an account, ensures the journal is gone and saves it in the settings.
      */
    Folder *addFolder(AccountState *accountState, const FolderDefinition &folderDefinition);

    /** Removes a folder */
    void removeFolder(Folder *folderToRemove);

    /** Returns the folder which the file or directory stored in path is in */
    Folder *folderForPath(const QString &path);

    // Takes local file paths and finds the corresponding folder, adding to correct selective sync list
    void whitelistFolderPath(const QString &path);
    void blacklistFolderPath(const QString &path);

    /**
      * returns a list of local files that exist on the local harddisk for an
      * incoming relative server path. The method checks with all existing sync
      * folders.
      */
    QStringList findFileInLocalFolders(const QString &relPath, const AccountPtr acc);

    /** Returns the folder by alias or \c nullptr if no folder with the alias exists. */
    Folder *folder(const QString &);

    /**
     * Migrate accounts from owncloud
     * Creates a folder for a specific configuration, identified by alias.
     */
    void setupLegacyFolder(const QString &, AccountState *account);

    /**
     * Ensures that a given directory does not contain a sync journal file.
     *
     * @returns false if the journal could not be removed, true otherwise.
     */
    static bool ensureJournalGone(const QString &journalDbFile);

    /** Creates a new and empty local directory. */
    bool startFromScratch(const QString &);

    /// Produce text for use in the tray tooltip
    static QString trayTooltipStatusString(SyncResult::Status syncStatus, bool hasUnresolvedConflicts, bool paused, ProgressInfo *progress);

    /// Compute status summarizing multiple folders
    static void trayOverallStatus(const QList<Folder *> &folders, SyncResult::Status *status, bool *unresolvedConflicts, ProgressInfo **overallProgressInfo);

    // Escaping of the alias which is used in QSettings AND the file
    // system, thus need to be escaped.
    static QString escapeAlias(const QString &);
    static QString unescapeAlias(const QString &);

    SocketApi *socketApi();

#ifdef Q_OS_WIN
    NavigationPaneHelper &navigationPaneHelper() { return _navigationPaneHelper; }
#endif

    /**
     * Check if @a path is a valid path for a new folder considering the already sync'ed items.
     * Make sure that this folder, or any subfolder is not sync'ed already.
     *
     * Note that different accounts are allowed to sync to the same folder.
     *
     * @returns an empty string and PathValidityResult::Valid if it is allowed, or an error if it is not allowed
     */
    [[nodiscard]] QPair<PathValidityResult, QString> checkPathValidityForNewFolder(const QString &path, const QUrl &serverUrl = QUrl()) const;

    /**
     * Attempts to find a non-existing, acceptable path for creating a new sync folder.
     *
     * Uses \a basePath as the baseline. It'll return this path if it's acceptable.
     *
     * Note that this can fail. If someone syncs ~ and \a basePath is ~/ownCloud, no
     * subfolder of ~ would be a good candidate. When that happens \a basePath
     * is returned.
     */
    [[nodiscard]] QString findGoodPathForNewSyncFolder(const QString &basePath, const QUrl &serverUrl, GoodPathStrategy allowExisting) const;

    /**
     * While ignoring hidden files can theoretically be switched per folder,
     * it's currently a global setting that users can only change for all folders
     * at once.
     * These helper functions can be removed once it's properly per-folder.
     */
    [[nodiscard]] bool ignoreHiddenFiles() const;
    void setIgnoreHiddenFiles(bool ignore);

    /**
     * Access to the current queue of scheduled folders.
     */
    [[nodiscard]] QQueue<Folder *> scheduleQueue() const;

    /**
     * Access to the currently syncing folder.
     *
     * Note: This is only the folder that's currently syncing *as-scheduled*. There
     * may be externally-managed syncs such as from placeholder hydrations.
     *
     * See also isAnySyncRunning()
     */
    [[nodiscard]] Folder *currentSyncFolder() const;

    /**
     * Returns true if any folder is currently syncing.
     *
     * This might be a FolderMan-scheduled sync, or a externally
     * managed sync like a placeholder hydration.
     */
    [[nodiscard]] bool isAnySyncRunning() const;

    /** Removes all folders */
    int unloadAndDeleteAllFolders();

    /**
     * If enabled is set to false, no new folders will start to sync.
     * The current one will finish.
     */
    void setSyncEnabled(bool);

    /** Queues a folder for syncing. */
    void scheduleFolder(Folder *);

    /** Queues a folder for syncing that starts immediately. */
    void scheduleFolderForImmediateSync(Folder *);

    /** Puts a folder in the very front of the queue. */
    void scheduleFolderNext(Folder *);

    /** Queues all folders for syncing. */
    void scheduleAllFolders();

    void setDirtyProxy();
    void setDirtyNetworkLimits();
    void setDirtyNetworkLimits(const AccountPtr &account) const;

    /** removes current user from the share **/
    void leaveShare(const QString &localFile);

    /** Whether or not vfs is supported in the location. */
    [[nodiscard]] bool checkVfsAvailability(const QString &path, Vfs::Mode mode = bestAvailableVfsMode()) const;

    /** If the folder configuration is no longer supported this will return an error string */
    [[nodiscard]] Result<void, QString> unsupportedConfiguration(const QString &path) const;
signals:
    /**
      * signal to indicate a folder has changed its sync state.
      *
      * Attention: The folder may be zero. Do a general update of the state then.
      */
    void folderSyncStateChange(OCC::Folder *);

    /**
     * Indicates when the schedule queue changes.
     */
    void scheduleQueueChanged();

    /**
     * Emitted whenever the list of configured folders changes.
     */
    void folderListChanged(const OCC::Folder::Map &);

    /**
     * Emitted once slotRemoveFoldersForAccount is done wiping
     */
    void wipeDone(OCC::AccountState *account, bool success);

public slots:

    /**
     * Schedules folders of newly connected accounts, terminates and
     * de-schedules folders of disconnected accounts.
     */
    void slotAccountStateChanged();

    /**
     * restart the client as soon as it is possible, ie. no folders syncing.
     */
    void slotScheduleAppRestart();

    /**
     * Triggers a sync run once the lock on the given file is removed.
     *
     * Automatically determines the folder that's responsible for the file.
     * See slotWatchedFileUnlocked().
     */
    void slotSyncOnceFileUnlocks(const QString &path);

    // slot to schedule an ETag job (from Folder only)
    void slotScheduleETagJob(const QString &alias, OCC::RequestEtagJob *job);

    /** Wipe folder */
    void slotWipeFolderForAccount(OCC::AccountState *accountState);

    void forceSyncForFolder(OCC::Folder *folder);

    void removeE2eFiles(const OCC::AccountPtr &account) const;

private slots:
    void slotFolderSyncPaused(OCC::Folder *, bool paused);
    void slotFolderCanSyncChanged();
    void slotFolderSyncStarted();
    void slotFolderSyncFinished(const OCC::SyncResult &);

    void slotRunOneEtagJob();
    void slotEtagJobDestroyed(QObject *);

    // slot to take the next folder from queue and start syncing.
    void slotStartScheduledFolderSync();
    void slotEtagPollTimerTimeout();

    void slotAccountRemoved(OCC::AccountState *accountState);

    void slotRemoveFoldersForAccount(OCC::AccountState *accountState);

    // Wraps the Folder::syncStateChange() signal into the
    // FolderMan::folderSyncStateChange(Folder*) signal.
    void slotForwardFolderSyncStateChange();

    void slotServerVersionChanged(const OCC::AccountPtr &account);

    /**
     * A file whose locks were being monitored has become unlocked.
     *
     * This schedules the folder for synchronization that contains
     * the file with the given path.
     */
    void slotWatchedFileUnlocked(const QString &path);

    /**
     * Schedules folders whose time to sync has come.
     *
     * Either because a long time has passed since the last sync or
     * because of previous failures.
     */
    void slotScheduleFolderByTime();

    void slotSetupPushNotifications(const OCC::Folder::Map &);
    void slotProcessFilesPushNotification(OCC::Account *account);
    void slotProcessFileIdsPushNotification(OCC::Account *account, const QList<qint64> &fileIds);
    void slotConnectToPushNotifications(const OCC::AccountPtr &account);

    void slotLeaveShare(const QString &localFile, const QByteArray &folderToken = {});

private:
    /** Adds a new folder, does not add it to the account settings and
     *  does not set an account on the new folder.
      */
    Folder *addFolderInternal(FolderDefinition folderDefinition,
        AccountState *accountState, std::unique_ptr<Vfs> vfs);

    /* unloads a folder object, does not delete it */
    void unloadFolder(Folder *);

    /** Will start a sync after a bit of delay. */
    void startScheduledSyncSoon();

    // finds all folder configuration files
    // and create the folders
    [[nodiscard]] QString getBackupName(QString fullPathName) const;

    // makes the folder known to the socket api
    void registerFolderWithSocketApi(Folder *folder);

    // restarts the application (Linux only)
    void restartApplication();

    void setupFoldersHelper(QSettings &settings, AccountStatePtr account, const QStringList &ignoreKeys, bool backwardsCompatible, bool foldersWithPlaceholders);

    void runEtagJobsIfPossible(const QList<Folder *> &folderMap);
    void runEtagJobIfPossible(Folder *folder);

    bool pushNotificationsFilesReady(const OCC::AccountPtr &account);

    [[nodiscard]] bool isSwitchToVfsNeeded(const FolderDefinition &folderDefinition) const;

    void addFolderToSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType list);

    QSet<Folder *> _disabledFolders;
    Folder::Map _folderMap;
    QString _folderConfigPath;
    Folder *_currentSyncFolder = nullptr;
    QPointer<Folder> _lastSyncFolder;
    bool _syncEnabled = true;

    /// Folder aliases from the settings that weren't read
    QSet<QString> _additionalBlockedFolderAliases;

    /// Starts regular etag query jobs
    QTimer _etagPollTimer;
    /// The currently running etag query
    QPointer<RequestEtagJob> _currentEtagJob;

    /// Watches files that couldn't be synced due to locks
    QScopedPointer<LockWatcher> _lockWatcher;

    /// Occasionally schedules folders
    QTimer _timeScheduler;

    /// Scheduled folders that should be synced as soon as possible
    QQueue<Folder *> _scheduledFolders;

    /// Picks the next scheduled folder and starts the sync
    QTimer _startScheduledSyncTimer;

    bool _nextSyncShouldStartImmediately = false;

    QScopedPointer<SocketApi> _socketApi;
#ifdef Q_OS_WIN
    NavigationPaneHelper _navigationPaneHelper;
#endif

    QPointer<UpdateE2eeFolderUsersMetadataJob> _removeE2eeShareJob;

    bool _appRestartRequired = false;

    static FolderMan *_instance;
    explicit FolderMan(QObject *parent = nullptr);
    friend class OCC::Application;
    friend class ::TestFolderMan;
    friend class ::TestSyncConflictsModel;
    friend class ::TestCfApiShellExtensionsIPC;
    friend class ::ShareTestHelper;
    friend class ::EndToEndTestHelper;
    friend class ::TestFolderStatusModel;
    friend class ::TestRemoteWipe;
    friend class ::FolderManTestHelper;
    friend class ::TestFileActionsModel;
};

} // namespace OCC
#endif // FOLDERMAN_H
