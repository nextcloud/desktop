/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_FOLDER_H
#define MIRALL_FOLDER_H

#include "syncresult.h"
#include "progressdispatcher.h"
#include "common/syncjournaldb.h"
#include "networkjobs.h"
#include "syncoptions.h"

#include <QObject>
#include <QStringList>
#include <QUuid>
#include <QSet>

#include <set>
#include <chrono>
#include <memory>

class QThread;
class QSettings;

namespace OCC {

class Vfs;
class SyncEngine;
class AccountState;
class SyncRunFileLog;
class FolderWatcher;
class LocalDiscoveryTracker;

/**
 * @brief The FolderDefinition class
 * @ingroup gui
 */
class FolderDefinition
{
public:
    /// The name of the folder in the ui and internally
    QString alias;
    /// path on local machine (always trailing /)
    QString localPath;
    /// path to the journal, usually relative to localPath
    QString journalPath;
    /// path on remote (usually no trailing /, exception "/")
    QString targetPath;
    /// whether the folder is paused
    bool paused = false;
    /// whether the folder syncs hidden files
    bool ignoreHiddenFiles = false;
    /// Which virtual files setting the folder uses
    Vfs::Mode virtualFilesMode = Vfs::Off;
    /// The CLSID where this folder appears in registry for the Explorer navigation pane entry.
    QUuid navigationPaneClsid;

    /// Whether the vfs mode shall silently be updated if possible
    bool upgradeVfsMode = false;

    /// Saves the folder definition into the current settings group.
    static void save(QSettings &settings, const FolderDefinition &folder);

    /// Reads a folder definition from the current settings group.
    static bool load(QSettings &settings, const QString &alias,
        FolderDefinition *folder);

    /** The highest version in the settings that load() can read
     *
     * Version 1: initial version (default if value absent in settings)
     * Version 2: introduction of metadata_parent hash in 2.6.0
     *            (version remains readable by 2.5.1)
     * Version 3: introduction of new windows vfs mode in 2.6.0
     * Version 5: available in oC client 4.0.0 and 4.2.0
     */
    static int maxSettingsVersion() { return 5; }

    /// Ensure / as separator and trailing /.
    static QString prepareLocalPath(const QString &path);

    /// Remove ending /, then ensure starting '/': so "/foo/bar" and "/".
    static QString prepareTargetPath(const QString &path);

    /// journalPath relative to localPath.
    [[nodiscard]] QString absoluteJournalPath() const;

    /// Returns the relative journal path that's appropriate for this folder and account.
    QString defaultJournalPath(AccountPtr account);
};

/**
 * @brief The Folder class
 * @ingroup gui
 */
class Folder : public QObject
{
    Q_OBJECT

public:
    enum class ChangeReason {
        Other,
        UnLock
    };
    Q_ENUM(ChangeReason)

    /** Create a new Folder
     */
    Folder(const FolderDefinition &definition, AccountState *accountState, std::unique_ptr<Vfs> vfs, QObject *parent = nullptr);

    ~Folder() override;

    using Map = QMap<QString, Folder *>;
    using MapIterator = QMapIterator<QString, Folder *>;

    /**
     * The account the folder is configured on.
     */
    AccountState *accountState() const { return _accountState.data(); }

    /**
     * alias or nickname
     */
    QString alias() const;
    QString shortGuiRemotePathOrAppName() const; // since 2.0 we don't want to show aliases anymore, show the path instead

    /**
     * short local path to display on the GUI  (native separators)
     */
    QString shortGuiLocalPath() const;

    /**
     * canonical local folder path, always ends with /
     */
    QString path() const;

    /**
     * cleaned canonical folder path, like path() but never ends with a /
     *
     * Wrapper for QDir::cleanPath(path()) except for "Z:/",
     * where it returns "Z:" instead of "Z:/".
     */
    QString cleanPath() const;

    /**
     * remote folder path, usually without trailing /, exception "/"
     */
    QString remotePath() const;

    /**
     * remote folder path, always with a trailing /
     */
    QString remotePathTrailingSlash() const;

    [[nodiscard]] QString fulllRemotePathToPathInSyncJournalDb(const QString &fullRemotePath) const;

    void setNavigationPaneClsid(const QUuid &clsid) { _definition.navigationPaneClsid = clsid; }
    QUuid navigationPaneClsid() const { return _definition.navigationPaneClsid; }

    /**
     * remote folder path with server url
     */
    QUrl remoteUrl() const;

    /**
     * switch sync on or off
     */
    void setSyncPaused(bool);

    bool syncPaused() const;

    /**
     * Returns true when the folder may sync.
     */
    bool canSync() const;

    void prepareToSync();

    /**
     * True if the folder is busy and can't initiate
     * a synchronization
     */
    virtual bool isBusy() const;

    /** True if the folder is currently synchronizing */
    bool isSyncRunning() const;

    /**
     * return the last sync result with error message and status
     */
    SyncResult syncResult() const;

    /**
      * This is called when the sync folder definition is removed. Do cleanups here.
      *
      * It removes the database, among other things.
      *
      * The folder is not in a valid state afterwards!
      */
    virtual void wipeForRemoval();

    void setSyncState(SyncResult::Status state);

    void setDirtyNetworkLimits();

    /**
      * Ignore syncing of hidden files or not. This is defined in the
      * folder definition
      */
    bool ignoreHiddenFiles();
    void setIgnoreHiddenFiles(bool ignore);

    // Used by the Socket API
    SyncJournalDb *journalDb() { return &_journal; }
    SyncEngine &syncEngine() { return *_engine; }
    Vfs &vfs() { return *_vfs; }

    RequestEtagJob *etagJob() { return _requestEtagJob; }
    std::chrono::milliseconds msecSinceLastSync() const { return std::chrono::milliseconds(_timeSinceLastSyncDone.elapsed()); }
    std::chrono::milliseconds msecLastSyncDuration() const { return _lastSyncDuration; }
    int consecutiveFollowUpSyncs() const { return _consecutiveFollowUpSyncs; }
    int consecutiveFailingSyncs() const { return _consecutiveFailingSyncs; }

    /// Saves the folder data in the account's settings.
    void saveToSettings() const;
    /// Removes the folder from the account's settings.
    void removeFromSettings() const;

    /* Check if the path is ignored. */
    [[nodiscard]] bool pathIsIgnored(const QString &path) const;

    /**
      * Returns whether a file inside this folder should be excluded.
      */
    bool isFileExcludedAbsolute(const QString &fullPath) const;

    /**
      * Returns whether a file inside this folder should be excluded.
      */
    bool isFileExcludedRelative(const QString &relativePath) const;

    /** Calls schedules this folder on the FolderMan after a short delay.
      *
      * This should be used in situations where a sync should be triggered
      * because a local file was modified. Syncs don't upload files that were
      * modified too recently, and this delay ensures the modification is
      * far enough in the past.
      *
      * The delay doesn't reset with subsequent calls.
      */
    void scheduleThisFolderSoon();

    void acceptInvalidFileName(const QString &filePath);

    void acceptCaseClashConflictFileName(const QString &filePath);

    /**
      * Migration: When this flag is true, this folder will save to
      * the backwards-compatible 'Folders' section in the config file.
      */
    void setSaveBackwardsCompatible(bool save);

    /** Used to have placeholders: save in placeholder config section */
    void setSaveInFoldersWithPlaceholders() { _saveInFoldersWithPlaceholders = true; }

    /**
     * Sets up this folder's folderWatcher if possible.
     *
     * May be called several times.
     */
    void registerFolderWatcher();

    /** virtual files of some kind are enabled
     *
     * This is independent of whether new files will be virtual. It's possible to have this enabled
     * and never have an automatic virtual file. But when it's on, the shell context menu will allow
     * users to make existing files virtual.
     */
    bool virtualFilesEnabled() const;
    void setVirtualFilesEnabled(bool enabled);

    void setRootPinState(PinState state);

    /** Whether user desires a switch that couldn't be executed yet, see member */
    bool isVfsOnOffSwitchPending() const { return _vfsOnOffPending; }
    void setVfsOnOffSwitchPending(bool pending) { _vfsOnOffPending = pending; }

    void switchToVirtualFiles();

    void processSwitchedToVirtualFiles();

    /** Whether this folder should show selective sync ui */
    bool supportsSelectiveSync() const;

    QString fileFromLocalPath(const QString &localPath) const;

    void whitelistPath(const QString &path);
    void blacklistPath(const QString &path);
    void migrateBlackListPath(const QString &legacyPath);

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const OCC::SyncResult &result);
    void progressInfo(const OCC::ProgressInfo &progress);
    void newBigFolderDiscovered(const QString &); // A new folder bigger than the threshold was discovered
    void syncPausedChanged(OCC::Folder *, bool paused);
    void canSyncChanged();

    /**
     * Fires for each change inside this folder that wasn't caused
     * by sync activity.
     */
    void watchedFileChangedExternally(const QString &path);

public slots:

    /**
       * terminate the current sync run
       */
    void slotTerminateSync();

    // connected to the corresponding signals in the SyncEngine
    void slotAboutToRemoveAllFiles(OCC::SyncFileItem::Direction, std::function<void(bool)> callback);

    /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
    void startSync(const QStringList &pathList = QStringList());

    int slotDiscardDownloadProgress();
    int downloadInfoCount();
    int slotWipeErrorBlacklist();
    int errorBlackListEntryCount();

    /**
       * Triggered by the folder watcher when a file/dir in this folder
       * changes. Needs to check whether this change should trigger a new
       * sync run to be scheduled.
       */
    void slotWatchedPathChanged(const QStringView &path, const OCC::Folder::ChangeReason reason);

    /*
    * Triggered when lock files were removed
    */
    void slotFilesLockReleased(const QSet<QString> &files);

    /*
     * Triggered when lock files were added
     */
    void slotFilesLockImposed(const QSet<QString> &files);

    void slotLockedFilesFound(const QSet<QString> &files);

    /**
     * Mark a virtual file as being requested for download, and start a sync.
     *
     * "implicit" here means that this download request comes from the user wanting
     * to access the file's data. The user did not change the file's pin state.
     * If the file is currently OnlineOnly its state will change to Unspecified.
     *
     * The download request is stored by setting ItemTypeVirtualFileDownload
     * in the database. This is necessary since the hydration is not driven by
     * the pin state.
     *
     * relativepath is the folder-relative path to the file (including the extension)
     *
     * Note, passing directories is not supported. Files only.
     */
    void implicitlyHydrateFile(const QString &relativepath);

    /** Adds the path to the local discovery list
     *
     * A weaker version of slotNextSyncFullLocalDiscovery() that just
     * schedules all parent and child items of the path for local
     * discovery.
     */
    void schedulePathForLocalDiscovery(const QString &relativePath);

    /** Ensures that the next sync performs a full local discovery. */
    void slotNextSyncFullLocalDiscovery();

    void setSilenceErrorsUntilNextSync(bool silenceErrors);

    /** Deletes local copies of E2EE files.
     * Intended for clean-up after disabling E2EE for an account.
     */
    void removeLocalE2eFiles();

private slots:
    void slotSyncStarted();
    void slotSyncFinished(bool);
    /*
     * Disconnects all the slots from the FolderWatcher
     * Needs to be called each time a folder is removed
     */
    void disconnectFolderWatcher();

    /** Adds a error message that's not tied to a specific item.
     */
    void slotSyncError(const QString &message, OCC::ErrorCategory category);

    void slotAddErrorToGui(OCC::SyncFileItem::Status status, const QString &errorMessage, const QString &subject, OCC::ErrorCategory category);

    void slotTransmissionProgress(const OCC::ProgressInfo &pi);
    void slotItemCompleted(const OCC::SyncFileItemPtr &, OCC::ErrorCategory errorCategory);

    void slotRunEtagJob();
    void etagRetrieved(const QByteArray &, const QDateTime &tp);
    void etagRetrievedFromSyncEngine(const QByteArray &, const QDateTime &time);

    void slotEmitFinishedDelayed();

    void slotNewBigFolderDiscovered(const QString &, bool isExternal);
    void slotExistingFolderNowBig(const QString &folderPath);

    void slotLogPropagationStart();

    /** Adds this folder to the list of scheduled folders in the
     *  FolderMan.
     */
    void slotScheduleThisFolder();

    /** Adjust sync result based on conflict data from IssuesWidget.
     *
     * This is pretty awkward, but IssuesWidget just keeps better track
     * of conflicts across partial local discovery.
     */
    void slotFolderConflicts(const QString &folder, const QStringList &conflictPaths);

    /** Warn users if they create a file or folder that is selective-sync excluded */
    void warnOnNewExcludedItem(const OCC::SyncJournalFileRecord &record, const QStringView &path);

    /** Warn users about an unreliable folder watcher */
    void slotWatcherUnreliable(const QString &message);

    /** Aborts any running sync and blocks it until hydration is finished.
     *
     * Hydration circumvents the regular SyncEngine and both mustn't be running
     * at the same time.
     */
    void slotHydrationStarts();

    /** Unblocks normal sync operation */
    void slotHydrationDone();

    /* Hydration failed, perform required steps to notify user */
    void slotHydrationFailed(int errorCode, int statusCode, const QString &errorString, const QString &fileName);

    void slotCapabilitiesChanged();

private:
    void connectSyncRoot();

    bool reloadExcludes();

    void showSyncResultPopup();

    void checkLocalPath();

    SyncOptions initializeSyncOptions() const;

    enum LogStatus {
        LogStatusRemove,
        LogStatusRename,
        LogStatusMove,
        LogStatusNew,
        LogStatusError,
        LogStatusConflict,
        LogStatusUpdated,
        LogStatusFileLocked
    };

    void createGuiLog(const QString &filename, LogStatus status, int count,
        const QString &renameTarget = QString());

    void startVfs();

    void correctPlaceholderFiles();

    void appendPathToSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType listType);
    void removePathFromSelectiveSyncList(const QString &path, const SyncJournalDb::SelectiveSyncListType listType);

    static void postExistingFolderNowBigNotification(const QString &folderPath);
    void postExistingFolderNowBigActivity(const QString &folderPath) const;

    AccountStatePtr _accountState;
    FolderDefinition _definition;
    QString _canonicalLocalPath; // As returned with QFileInfo:canonicalFilePath.  Always ends with "/"

    SyncResult _syncResult;
    QScopedPointer<SyncEngine> _engine;
    QPointer<RequestEtagJob> _requestEtagJob;
    QByteArray _lastEtag;
    QElapsedTimer _timeSinceLastSyncDone;
    QElapsedTimer _timeSinceLastSyncStart;
    QElapsedTimer _timeSinceLastFullLocalDiscovery;
    std::chrono::milliseconds _lastSyncDuration;

    /// The number of syncs that failed in a row.
    /// Reset when a sync is successful.
    int _consecutiveFailingSyncs = 0;

    /// The number of requested follow-up syncs.
    /// Reset when no follow-up is requested.
    int _consecutiveFollowUpSyncs = 0;

    mutable SyncJournalDb _journal;

    QScopedPointer<SyncRunFileLog> _fileLog;

    QTimer _scheduleSelfTimer;

    /**
     * When the same local path is synced to multiple accounts, only one
     * of them can be stored in the settings in a way that's compatible
     * with old clients that don't support it. This flag marks folders
     * that shall be written in a backwards-compatible way, by being set
     * on the *first* Folder instance that was configured for each local
     * path.
     */
    bool _saveBackwardsCompatible = false;

    /** Whether the folder should be saved in that settings group
     *
     * If it was read from there it had virtual files enabled at some
     * point and might still have db entries or suffix-virtual files even
     * if they are disabled right now. This flag ensures folders that
     * were in that group once never go back.
     */
    bool _saveInFoldersWithPlaceholders = false;

    /** Whether a vfs mode switch is pending
     *
     * When the user desires that vfs be switched on/off but it hasn't been
     * executed yet (syncs are still running), some options should be hidden,
     * disabled or different.
     */
    bool _vfsOnOffPending = false;

    /** Whether this folder has just switched to VFS or not
     */
    bool _hasSwitchedToVfs = false;

    bool _silenceErrorsUntilNextSync = false;

    /**
     * Watches this folder's local directory for changes.
     *
     * Created by registerFolderWatcher(), triggers slotWatchedPathChanged()
     */
    QScopedPointer<FolderWatcher> _folderWatcher;

    /**
     * Keeps track of locally dirty files so we can skip local discovery sometimes.
     */
    QScopedPointer<LocalDiscoveryTracker> _localDiscoveryTracker;

    /**
     * The vfs mode instance (created by plugin) to use. Never null.
     */
    QSharedPointer<Vfs> _vfs;

    QMetaObject::Connection _officeFileLockReleaseUnlockSuccess;
    QMetaObject::Connection _officeFileLockReleaseUnlockFailure;
    QMetaObject::Connection _fileLockSuccess;
    QMetaObject::Connection _fileLockFailure;
};
}

#endif
