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

#include "accountstate.h"
#include "common/syncjournaldb.h"
#include "networkjobs.h"
#include "progressdispatcher.h"
#include "syncoptions.h"
#include "syncresult.h"

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QUuid>

#include <chrono>
#include <memory>
#include <set>

class QThread;
class QSettings;

namespace OCC {

class Vfs;
class SyncEngine;
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
    static auto createNewFolderDefinition(const QUrl &davUrl, const QString &displayName = {})
    {
        return FolderDefinition(QUuid::createUuid().toByteArray(QUuid::WithoutBraces), davUrl, displayName);
    }

    /// path to the journal, usually relative to localPath
    QString journalPath;

    /// whether the folder is paused
    bool paused = false;
    /// whether the folder syncs hidden files
    bool ignoreHiddenFiles = true;
    /// Which virtual files setting the folder uses
    Vfs::Mode virtualFilesMode = Vfs::Off;

    /// Whether the vfs mode shall silently be updated if possible
    bool upgradeVfsMode = false;

    /// Saves the folder definition into the current settings group.
    static void save(QSettings &settings, const FolderDefinition &folder);

    /// Reads a folder definition from the current settings group.
    static FolderDefinition load(QSettings &settings, const QByteArray &id);

    /** The highest version in the settings that load() can read
     *
     * Version 1: initial version (default if value absent in settings)
     * Version 2: introduction of metadata_parent hash in 2.6.0
     *            (version remains readable by 2.5.1)
     * Version 3: introduction of new windows vfs mode in 2.6.0
     * Version 4: until 2.9.1 windows vfs tried to unregister folders with a different id from windows.
     * Version 5: 3.0.0 Introduced spaces, the profiles are not downwards compatible
     */
    static int maxSettingsVersion();

    /// Ensure / as separator and trailing /.
    void setLocalPath(const QString &path);

    /// Remove ending /, then ensure starting '/': so "/foo/bar" and "/".
    void setTargetPath(const QString &path);

    /// journalPath relative to localPath.
    QString absoluteJournalPath() const;

    QString localPath() const
    {
        return _localPath;
    }
    QString targetPath() const
    {
        return _targetPath;
    }
    const QUrl &webDavUrl() const
    {
        Q_ASSERT(_webDavUrl.isValid());
        return _webDavUrl;
    }

    const QByteArray &id() const;

    QString displayName() const;

    /**
     * The folder is deployed by an admin
     * We will hide the remove option and the disable/enable vfs option.
     */
    bool isDeployed() const;


    /**
     * Higher values mean more imortant
     * Used for sorting
     */
    uint32_t priority() const;

    void setPriority(uint32_t newPriority);

private:
    FolderDefinition(const QByteArray &id, const QUrl &davUrl, const QString &displayName);

    QUrl _webDavUrl;
    /// For legacy reasons this can be a string, new folder objects will use a uuid
    QByteArray _id;
    QString _displayName;
    /// path on local machine (always trailing /)
    QString _localPath;
    /// path on remote (usually no trailing /, exception "/")
    QString _targetPath;
    bool _deployed = false;

    uint32_t _priority = 0;

    friend class FolderMan;
    friend class SpaceMigration;
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

    static void prepareFolder(const QString &path);

    /** Create a new Folder
     */
    Folder(const FolderDefinition &definition, const AccountStatePtr &accountState, std::unique_ptr<Vfs> &&vfs, QObject *parent = nullptr);

    ~Folder() override;
    /**
     * The account the folder is configured on.
     */
    AccountStatePtr accountState() const { return _accountState; }

    QByteArray id() const;

    QString displayName() const;

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
     * The full remote webdav url
     */
    QUrl webDavUrl() const;

    /**
     * remote folder path, always with a trailing /
     */
    QString remotePathTrailingSlash() const;

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

    /**
     * Whether the folder is ready
     */
    bool isReady() const;

    bool hasSetupError() const
    {
        return _syncResult.status() == SyncResult::SetupError;
    }

    /**
     *  Returns true if the folder needs sync poll interval wise, and can
     *  sync due to its internal state
     */
    bool dueToSync() const;

    void prepareToSync();

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

    // TODO: don't expose
    SyncJournalDb *journalDb()
    {
        return &_journal;
    }
    // TODO: don't expose
    SyncEngine &syncEngine()
    {
        return *_engine;
    }
    Vfs &vfs()
    {
        OC_ENFORCE(_vfs);
        return *_vfs;
    }

    RequestEtagJob *etagJob() const { return _requestEtagJob; }
    auto lastSyncTime() const { return QDateTime::currentDateTime().addMSecs(-msecSinceLastSync().count()); }
    std::chrono::milliseconds msecSinceLastSync() const { return std::chrono::milliseconds(_timeSinceLastSyncDone.elapsed()); }
    std::chrono::milliseconds msecLastSyncDuration() const { return _lastSyncDuration; }
    int consecutiveFollowUpSyncs() const { return _consecutiveFollowUpSyncs; }
    int consecutiveFailingSyncs() const { return _consecutiveFailingSyncs; }

    /// Saves the folder data in the account's settings.
    void saveToSettings() const;
    /// Removes the folder from the account's settings.
    void removeFromSettings() const;

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

    /** Whether user desires a switch that couldn't be executed yet, see member */
    bool isVfsOnOffSwitchPending() const { return _vfsOnOffPending; }
    void setVfsOnOffSwitchPending(bool pending) { _vfsOnOffPending = pending; }

    /** Whether this folder should show selective sync ui */
    bool supportsSelectiveSync() const;

    /**
     * Whether to register the parent folder of our sync root in the explorer
     * The default behaviour is to register alls spaces in a common dir in the home folder
     * in that case we only display that common dir in the Windows side bar.
     * With the legacy behaviour we only have one dir which we will register with Windows
     */
    bool groupInSidebar() const;

    /**
     * The folder is deployed by an admin
     * We will hide the remove option and the disable/enable vfs option.
     */
    bool isDeployed() const;

    auto priority()
    {
        return _definition.priority();
    }

    void setPriority(uint32_t p)
    {
        return _definition.setPriority(p);
    }

signals:
    void syncStateChange();
    void syncStarted();
    void syncFinished(const SyncResult &result);
    void newBigFolderDiscovered(const QString &); // A new folder bigger than the threshold was discovered
    void syncPausedChanged(Folder *, bool paused);
    void canSyncChanged();

    /**
     * Fires for each change inside this folder that wasn't caused
     * by sync activity.
     */
    void watchedFileChangedExternally(const QString &path);

public slots:

    void slotRunEtagJob();

    /**
       * terminate the current sync run
       */
    void slotTerminateSync();

    // connected to the corresponding signals in the SyncEngine
    void slotAboutToRemoveAllFiles(SyncFileItem::Direction);

    /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
    void startSync();

    int slotDiscardDownloadProgress();
    int downloadInfoCount();
    int slotWipeErrorBlacklist();
    int errorBlackListEntryCount();

    /**
       * Triggered by the folder watcher when a file/dir in this folder
       * changes. Needs to check whether this change should trigger a new
       * sync run to be scheduled.
       */
    void slotWatchedPathsChanged(const QSet<QString> &paths, ChangeReason reason);

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

    /** Ensures that the next sync performs a full local discovery. */
    void slotNextSyncFullLocalDiscovery();

    /** Adds the path to the local discovery list
     *
     * A weaker version of slotNextSyncFullLocalDiscovery() that just
     * schedules all parent and child items of the path for local
     * discovery.
     */
    void schedulePathForLocalDiscovery(const QString &relativePath);

    /// Reloads the excludes, used when changing the user-defined excludes after saving them to disk.
    bool reloadExcludes();

private slots:
    void slotSyncStarted();
    void slotSyncFinished(bool);

    /** Adds a error message that's not tied to a specific item.
     */
    void slotSyncError(const QString &message, ErrorCategory category = ErrorCategory::Normal);

    void slotItemCompleted(const SyncFileItemPtr &);

    void slotEmitFinishedDelayed();

    void slotNewBigFolderDiscovered(const QString &, bool isExternal);

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
    void slotFolderConflicts(Folder *folder, const QStringList &conflictPaths);

    /** Warn users if they create a file or folder that is selective-sync excluded */
    void warnOnNewExcludedItem(const SyncJournalFileRecord &record, QStringView path);

    /** Warn users about an unreliable folder watcher */
    void slotWatcherUnreliable(const QString &message);

private:
    void showSyncResultPopup();

    bool checkLocalPath();

    SyncOptions loadSyncOptions();

    enum LogStatus {
        LogStatusRemove,
        LogStatusRename,
        LogStatusMove,
        LogStatusNew,
        LogStatusError,
        LogStatusConflict,
        LogStatusUpdated
    };

    void createGuiLog(const QString &filename, LogStatus status, int count,
        const QString &renameTarget = QString());

    void startVfs();

    AccountStatePtr _accountState;
    FolderDefinition _definition;
    QString _canonicalLocalPath; // As returned with QFileInfo:canonicalFilePath.  Always ends with "/"

    SyncResult _syncResult;
    QScopedPointer<SyncEngine> _engine;
    QPointer<RequestEtagJob> _requestEtagJob;
    QString _lastEtag;
    QElapsedTimer _timeSinceLastEtagCheckDone;
    QElapsedTimer _timeSinceLastSyncDone;
    QElapsedTimer _timeSinceLastSyncStart;
    QElapsedTimer _timeSinceLastFullLocalDiscovery;
    std::chrono::milliseconds _lastSyncDuration;

    /// The number of syncs that failed in a row.
    /// Reset when a sync is successful.
    int _consecutiveFailingSyncs;

    /// The number of requested follow-up syncs.
    /// Reset when no follow-up is requested.
    int _consecutiveFollowUpSyncs;

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

    /**
     * Setting up vfs is a async operation
     */
    bool _vfsIsReady = false;

    /**
     * Watches this folder's local directory for changes.
     *
     * Created by registerFolderWatcher(), triggers slotWatchedPathsChanged()
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

    // allow that all files are removed in the next run
    bool _allowRemoveAllOnce = false;

    friend class SpaceMigration;
};
}

#endif
