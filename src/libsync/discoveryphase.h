/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "networkjobs.h"
#include "syncoptions.h"
#include "syncfileitem.h"

#include "common/folderquota.h"
#include "common/remoteinfo.h"

#include <QObject>
#include <QElapsedTimer>
#include <QStringList>
#include <csync.h>
#include <QMap>
#include <QSet>
#include <QMutex>
#include <QWaitCondition>
#include <QRunnable>
#include <deque>

class ExcludedFiles;

namespace OCC {

namespace LocalDiscoveryEnums {

OCSYNC_EXPORT Q_NAMESPACE

enum class LocalDiscoveryStyle {
    FilesystemOnly, //< read all local data from the filesystem
    DatabaseAndFilesystem, //< read from the db, except for listed paths
};

Q_ENUM_NS(LocalDiscoveryStyle)

}

using OCC::LocalDiscoveryEnums::LocalDiscoveryStyle;

class Account;
class SyncJournalDb;
class ProcessDirectoryJob;

enum class ErrorCategory;

struct LocalInfo
{
    /** FileName of the entry (this does not contains any directory or path, just the plain name */
    QString name;
    QString caseClashConflictingName;
    time_t modtime = 0;
    int64_t size = 0;
    uint64_t inode = 0;
    ItemType type = ItemTypeSkip;
    bool isDirectory = false;
    bool isHidden = false;
    bool isVirtualFile = false;
    bool isSymLink = false;
    bool isMetadataMissing = false;
    bool isPermissionsInvalid = false;
    [[nodiscard]] bool isValid() const { return !name.isNull(); }
};

/**
 * @brief Run list on a local directory and process the results for Discovery
 *
 * @ingroup libsync
 */
class DiscoverySingleLocalDirectoryJob : public QObject, public QRunnable
{
    Q_OBJECT
public:
    explicit DiscoverySingleLocalDirectoryJob(const AccountPtr &account,
                                              const QString &localPath,
                                              OCC::Vfs *vfs,
                                              bool fileSystemReliablePermissions,
                                              QObject *parent = nullptr);

    void run() override;
signals:
    void finished(QVector<OCC::LocalInfo> result);
    void finishedFatalError(QString errorString);
    void finishedNonFatalError(QString errorString);

    void itemDiscovered(OCC::SyncFileItemPtr item);
    void childIgnored(bool b);
private slots:
private:
    QString _localPath;
    AccountPtr _account;
    OCC::Vfs* _vfs;
    bool _fileSystemReliablePermissions = false;
public:
};

class FolderMetadata;

/**
 * @brief Run a PROPFIND on a directory and process the results for Discovery
 *
 * @ingroup libsync
 */
class DiscoverySingleDirectoryJob : public QObject
{
    Q_OBJECT
public:
    explicit DiscoverySingleDirectoryJob(const AccountPtr &account,
                                         const QString &path,
                                         const QString &remoteRootFolderPath,
        /* TODO for topLevelE2eeFolderPaths, from review: I still do not get why giving the whole QSet instead of just the parent of the folder we are in
        sounds to me like it would be much more efficient to just have the e2ee parent folder that we are
        inside*/
                                         const QSet<QString> &topLevelE2eeFolderPaths,
                                         SyncFileItem::EncryptionStatus parentEncryptionStatus,
                                         QObject *parent = nullptr);
    // Specify that this is the root and we need to check the data-fingerprint
    void setIsRootPath() { _isRootPath = true; }
    void start();
    void abort();
    [[nodiscard]] bool isFileDropDetected() const;
    [[nodiscard]] bool encryptedMetadataNeedUpdate() const;
    [[nodiscard]] SyncFileItem::EncryptionStatus currentEncryptionStatus() const;
    [[nodiscard]] SyncFileItem::EncryptionStatus requiredEncryptionStatus() const;

    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(OCC::RemotePermissions);
    void etag(const QByteArray &, const QDateTime &time);
    void finished(const OCC::HttpResult<QVector<OCC::RemoteInfo>> &result);
    void setfolderQuota(const OCC::FolderQuota &folderQuota);

private slots:
    void directoryListingIteratedSlot(const QString &, const QMap<QString, QString> &);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply *reply);
    void fetchE2eMetadata();
    void metadataReceived(const QJsonDocument &json, int statusCode);
    void metadataError(const QByteArray& fileId, int httpReturnCode);

private:

    [[nodiscard]] bool isE2eEncrypted() const { return _encryptionStatusCurrent != SyncFileItem::EncryptionStatus::NotEncrypted; }

    QVector<RemoteInfo> _results;
    QString _subPath;
    QString _remoteRootFolderPath;
    QByteArray _firstEtag;
    QByteArray _fileId;
    QByteArray _localFileId;
    AccountPtr _account;
    // The first result is for the directory itself and need to be ignored.
    // This flag is true if it was already ignored.
    bool _ignoredFirst = false;
    // Set to true if this is the root path and we need to check the data-fingerprint
    bool _isRootPath = false;
    // If this directory is an external storage (The first item has 'M' in its permission)
    bool _isExternalStorage = false;
    // If this directory is e2ee
    SyncFileItem::EncryptionStatus _encryptionStatusCurrent = SyncFileItem::EncryptionStatus::NotEncrypted;
    bool _isFileDropDetected = false;
    bool _encryptedMetadataNeedUpdate = false;
    SyncFileItem::EncryptionStatus _encryptionStatusRequired = SyncFileItem::EncryptionStatus::NotEncrypted;

    // If set, the discovery will finish with an error
    int64_t _size = 0;
    QString _error;
    QPointer<LsColJob> _lsColJob;

    // store top level E2EE folder paths as they are used later when discovering nested folders
    QSet<QString> _topLevelE2eeFolderPaths;

public:
    QByteArray _dataFingerprint;
    FolderQuota _folderQuota;
};

class DiscoveryPhase : public QObject
{
    Q_OBJECT

    friend class ProcessDirectoryJob;

    QPointer<ProcessDirectoryJob> _currentRootJob;

    /** Maps the db-path of a deleted item to its SyncFileItem.
     *
     * If it turns out the item was renamed after all, the instruction
     * can be changed. See findAndCancelDeletedJob(). Note that
     * itemDiscovered() will already have been emitted for the item.
     */
    QMap<QString, SyncFileItemPtr> _deletedItem;

    QVector<QString> _directoryNamesToRestoreOnPropagation;

    /** Maps the db-path of a deleted folder to its queued job.
     *
     * If a folder is deleted and must be recursed into, its job isn't
     * executed immediately. Instead it's queued here and only run
     * once the rest of the discovery has finished and we are certain
     * that the folder wasn't just renamed. This avoids running the
     * discovery on contents in the old location of renamed folders.
     *
     * See findAndCancelDeletedJob().
     */
    QMap<QString, ProcessDirectoryJob *> _queuedDeletedDirectories;

    // map source (original path) -> destinations (current server or local path)
    QMap<QString, QString> _renamedItemsRemote;
    QMap<QString, QString> _renamedItemsLocal;

    // set of paths that should not be removed even though they are removed locally:
    // there was a move to an invalid destination and now the source should be restored
    //
    // This applies recursively to subdirectories.
    // All entries should have a trailing slash (even files), so lookup with
    // lowerBound() is reliable.
    //
    // The value of this map doesn't matter.
    QMap<QString, bool> _forbiddenDeletes;

    /** Returns whether the db-path has been renamed locally or on the remote.
     *
     * Useful for avoiding processing of items that have already been claimed in
     * a rename (would otherwise be discovered as deletions).
     */
    [[nodiscard]] bool isRenamed(const QString &p) const;

    int _currentlyActiveJobs = 0;

    // both must contain a sorted list
    QStringList _selectiveSyncBlackList;
    QStringList _selectiveSyncWhiteList;

    void scheduleMoreJobs();

    [[nodiscard]] bool isInSelectiveSyncBlackList(const QString &path) const;

    [[nodiscard]] bool activeFolderSizeLimit() const;
    [[nodiscard]] bool notifyExistingFolderOverLimit() const;

    void checkFolderSizeLimit(const QString &path,
			      const std::function<void(bool)> callback);

    // Check if the new folder should be deselected or not.
    // May be async. "Return" via the callback, true if the item is blacklisted
    void checkSelectiveSyncNewFolder(const QString &path,
                                     const RemotePermissions rp,
                                     const std::function<void(bool)> callback);

    void checkSelectiveSyncExistingFolder(const QString &path);

    /** Given an original path, return the target path obtained when renaming is done.
     *
     * Note that it only considers parent directory renames. So if A/B got renamed to C/D,
     * checking A/B/file would yield C/D/file, but checking A/B would yield A/B.
     */
    [[nodiscard]] QString adjustRenamedPath(const QString &original, SyncFileItem::Direction) const;

    /** If the db-path is scheduled for deletion, abort it.
     *
     * Check if there is already a job to delete that item:
     * If that's not the case, return { false, QByteArray() }.
     * If there is such a job, cancel that job and return true and the old etag.
     *
     * Used when having detected a rename: The rename source may have been
     * discovered before and would have looked like a delete.
     *
     * See _deletedItem and _queuedDeletedDirectories.
     */
    QPair<bool, QByteArray> findAndCancelDeletedJob(const QString &originalPath);

    void enqueueDirectoryToDelete(const QString &path, ProcessDirectoryJob* const directoryJob);

    bool recursiveCheckForDeletedParents(const QString &itemPath) const;

    /// contains files/folder names that are requested to be deleted permanently
    QSet<QString> _permanentDeletionRequests;

    void markPermanentDeletionRequests();

public:
    // input
    QString _localDir; // absolute path to the local directory. ends with '/'
    QString _remoteFolder; // remote folder, ends with '/'
    SyncJournalDb *_statedb = nullptr;
    AccountPtr _account;
    SyncOptions _syncOptions;
    ExcludedFiles *_excludes = nullptr;
    QRegularExpression _invalidFilenameRx; // FIXME: maybe move in ExcludedFiles
    QStringList _serverBlacklistedFiles; // The blacklist from the capabilities
    QStringList _leadingAndTrailingSpacesFilesAllowed;
    bool _shouldEnforceWindowsFileNameCompatibility = false;
    bool _ignoreHiddenFiles = false;
    std::function<bool(const QString &)> _shouldDiscoverLocaly;

    void startJob(ProcessDirectoryJob *);

    void setSelectiveSyncBlackList(const QStringList &list);
    void setSelectiveSyncWhiteList(const QStringList &list);

    // output
    QByteArray _dataFingerprint;
    bool _anotherSyncNeeded = false;
    QHash<QString, long long> _filesNeedingScheduledSync;
    QVector<QString> _filesUnscheduleSync;

    QStringList _listExclusiveFiles;

    QStringList _forbiddenFilenames;
    QStringList _forbiddenBasenames;
    QStringList _forbiddenExtensions;
    QStringList _forbiddenChars;

    bool _hasUploadErrorItems = false;
    bool _hasDownloadRemovedItems = false;

    bool _noCaseConflictRecordsInDb = false;

    bool _fileSystemReliablePermissions = false;

    QSet<QString> _topLevelE2eeFolderPaths;

signals:
    void fatalError(const QString &errorString, const OCC::ErrorCategory errorCategory);
    void itemDiscovered(const OCC::SyncFileItemPtr &item);
    void finished();

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);
    void existingFolderNowBig(const QString &folder);

    /** For excluded items that don't show up in itemDiscovered()
      *
      * The path is relative to the sync folder, similar to item->_file
      */
    void silentlyExcluded(const QString &folderPath);

    void addErrorToGui(const OCC::SyncFileItem::Status status, const QString &errorMessage, const QString &subject, const OCC::ErrorCategory category);

    void remnantReadOnlyFolderDiscovered(const OCC::SyncFileItemPtr &item);
private slots:
    void slotItemDiscovered(const OCC::SyncFileItemPtr &item);
};

/// Implementation of DiscoveryPhase::adjustRenamedPath
QString adjustRenamedPath(const QMap<QString, QString> &renamedItems, const QString &original);
}
