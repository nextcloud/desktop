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
    static auto createNewFolderDefinition(const QUrl &davUrl, const QString &spaceId, const QString &displayName = {})
    {
        return FolderDefinition(QUuid::createUuid().toByteArray(QUuid::WithoutBraces), davUrl, spaceId, displayName);
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

    /// Ensure / as separator and trailing /.
    void setLocalPath(const QString &path);

    /// Remove ending /, then ensure starting '/': so "/foo/bar" and "/".
    void setTargetPath(const QString &path);

    /// journalPath relative to localPath.
    QString absoluteJournalPath() const;

    QString localPath() const;

    QString targetPath() const;

    QUrl webDavUrl() const;

    // could change in the case of spaces
    void setWebDavUrl(const QUrl &url) { _webDavUrl = url; }

    // when using spaces we don't store the dav url but the space id
    // this id is then used to look up the dav url
    QString spaceId() const;

    void setSpaceId(const QString &spaceId) { _spaceId = spaceId; }

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
    FolderDefinition(const QByteArray &id, const QUrl &davUrl, const QString &spaceId, const QString &displayName);

    // oc10 and as cache for ocis
    QUrl _webDavUrl;

    QString _spaceId;
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
     * The space Id (empty for oc10)
     */
    QString spaceId() const;

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

    void reloadSyncOptions();

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

    auto lastSyncTime() const { return QDateTime::currentDateTime().addMSecs(-msecSinceLastSync().count()); }
    std::chrono::milliseconds msecSinceLastSync() const { return std::chrono::milliseconds(_timeSinceLastSyncDone.elapsed()); }
    std::chrono::milliseconds msecLastSyncDuration() const { return _lastSyncDuration; }

    /// Saves the folder data in the account's settings.
    void saveToSettings() const;
    /// Removes the folder from the account's settings.
    static void removeFromSettings(QSettings *settings, const QString &id);
    void removeFromSettings() const;

    /**
      * Returns whether a file inside this folder should be excluded.
      */
    bool isFileExcludedAbsolute(const QString &fullPath) const;

    /**
      * Returns whether a file inside this folder should be excluded.
      */
    bool isFileExcludedRelative(const QString &relativePath) const;

    /** virtual files of some kind are enabled
     *
     * This is independent of whether new files will be virtual. It's possible to have this enabled
     * and never have an automatic virtual file. But when it's on, the shell context menu will allow
     * users to make existing files virtual.
     */
    bool virtualFilesEnabled() const;
    void setVirtualFilesEnabled(bool enabled);

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

    static Result<void, QString> checkPathLength(const QString &path);

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
    /**
       * terminate the current sync run
       */
    void slotTerminateSync(const QString &reason);

    // connected to the corresponding signals in the SyncEngine
    void slotAboutToRemoveAllFiles(SyncFileItem::Direction);

    /**
      * Starts a sync operation
      *
      * If the list of changed files is known, it is passed.
      */
    void startSync();

    void slotDiscardDownloadProgress();
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

    void slotLogPropagationStart();

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

    /**
     * Sets up this folder's folderWatcher if possible.
     *
     * May be called several times.
     */
    void registerFolderWatcher();

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
    QElapsedTimer _timeSinceLastSyncDone;
    QElapsedTimer _timeSinceLastSyncStart;
    QElapsedTimer _timeSinceLastFullLocalDiscovery;
    std::chrono::milliseconds _lastSyncDuration = {};

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

    QPointer<QMessageBox> _removeAllFilesDialog;

    // allow that all files are removed in the next run
    bool _allowRemoveAllOnce = false;

    friend class SpaceMigration;
};
}

#endif
