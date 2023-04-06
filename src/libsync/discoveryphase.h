/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QStringList>
#include <csync.h>
#include <QMap>
#include <QSet>
#include "networkjobs.h"
#include <QMutex>
#include <QWaitCondition>
#include <QRunnable>
#include <deque>
#include "syncoptions.h"
#include "syncfileitem.h"

#include "csync/csync_exclude.h"

namespace OCC {

enum class LocalDiscoveryStyle {
    FilesystemOnly, //< read all local data from the filesystem
    DatabaseAndFilesystem, //< read from the db, except for listed paths
};


class Account;
class SyncJournalDb;
class ProcessDirectoryJob;

/**
 * Represent all the meta-data about a file in the server
 */
struct RemoteInfo
{
    /** FileName of the entry (this does not contains any directory or path, just the plain name */
    QString name;
    QString etag;
    QByteArray fileId;
    QByteArray checksumHeader;
    OCC::RemotePermissions remotePerm;
    time_t modtime = 0;
    int64_t size = 0;
    bool isDirectory = false;
    bool isValid() const { return !name.isNull(); }

    QString directDownloadUrl;
    QString directDownloadCookies;
};

struct LocalInfo
{
    /** FileName of the entry (this does not contains any directory or path, just the plain name */
    QString name;
    time_t modtime = 0;
    int64_t size = 0;
    uint64_t inode = 0;
    ItemType type = ItemTypeSkip;
    bool isDirectory = false;
    bool isHidden = false;
    bool isVirtualFile = false;
    bool isSymLink = false;
    bool isValid() const { return !name.isNull(); }
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
    explicit DiscoverySingleLocalDirectoryJob(const AccountPtr &account, const QString &localPath, OCC::Vfs *vfs, QObject *parent = nullptr);

    void run() override;
signals:
    void finished(QVector<LocalInfo> result);
    void finishedFatalError(QString errorString);
    void finishedNonFatalError(QString errorString);

    void itemDiscovered(SyncFileItemPtr item);
    void childIgnored(bool b);
private slots:
private:
    QString _localPath;
    AccountPtr _account;
    OCC::Vfs* _vfs;
public:
};


/**
 * @brief Run a PROPFIND on a directory and process the results for Discovery
 *
 * @ingroup libsync
 */
class DiscoverySingleDirectoryJob : public QObject
{
    Q_OBJECT
public:
    explicit DiscoverySingleDirectoryJob(const AccountPtr &account, const QUrl &baseUrl, const QString &path, QObject *parent = nullptr);
    // Specify that this is the root and we need to check the data-fingerprint
    void setIsRootPath() { _isRootPath = true; }
    bool isRootPath() const { return _isRootPath; }
    void start();
    void abort();

    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(RemotePermissions);
    void etag(const QString &, const QDateTime &time);
    void finished(const HttpResult<QVector<RemoteInfo>> &result);

private slots:
    void directoryListingIteratedSlot(const QString &, const QMap<QString, QString> &);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply *);

private:
    QVector<RemoteInfo> _results;
    QString _subPath;
    QString _firstEtag;
    AccountPtr _account;
    const QUrl _baseUrl;
    // The first result is for the directory itself and need to be ignored.
    // This flag is true if it was already ignored.
    bool _ignoredFirst;
    // Set to true if this is the root path and we need to check the data-fingerprint
    bool _isRootPath;
    // If this directory is an external storage (The first item has 'M' in its permission)
    bool _isExternalStorage;
    // If set, the discovery will finish with an error
    QString _error;
    QPointer<PropfindJob> _proFindJob;

public:
    QByteArray _dataFingerprint;
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
    QHash<QString, SyncFileItemPtr> _deletedItem;

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
    // needs to be ordered
    QMap<QString, ProcessDirectoryJob *> _queuedDeletedDirectories;

    // map source (original path) -> destinations (current server or local path)
    QHash<QString, QString> _renamedItemsRemote;
    QHash<QString, QString> _renamedItemsLocal;

    // set of paths that should not be removed even though they are removed locally:
    // there was a move to an invalid destination and now the source should be restored
    //
    // This applies recursively to subdirectories.
    // All entries should have a trailing slash (even files), so lookup with
    // lowerBound() is reliable.
    //
    // needs to be sorted
    std::set<QString> _forbiddenDeletes;

    /** Returns whether the db-path has been renamed locally or on the remote.
     *
     * Useful for avoiding processing of items that have already been claimed in
     * a rename (would otherwise be discovered as deletions).
     */
    bool isRenamed(const QString &p) const { return _renamedItemsLocal.contains(p) || _renamedItemsRemote.contains(p); }

    int _currentlyActiveJobs = 0;

    // both must contain a sorted list
    std::set<QString> _selectiveSyncBlackList;
    std::set<QString> _selectiveSyncWhiteList;

    void scheduleMoreJobs();

    bool isInSelectiveSyncBlackList(const QString &path) const;

    // Check if the new folder should be deselected or not.
    // May be async. "Return" via the callback, true if the item is blacklisted
    void checkSelectiveSyncNewFolder(const QString &path, RemotePermissions rp,
        const std::function<void(bool)> &callback);

    /** Given an original path, return the target path obtained when renaming is done.
     *
     * Note that it only considers parent directory renames. So if A/B got renamed to C/D,
     * checking A/B/file would yield C/D/file, but checking A/B would yield A/B.
     */
    QString adjustRenamedPath(const QString &original, SyncFileItem::Direction) const;

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
    QPair<bool, QString> findAndCancelDeletedJob(const QString &originalPath);

public:
    // input
    DiscoveryPhase(const AccountPtr &account, const SyncOptions &options, const QUrl &baseUrl, QObject *parent = nullptr)
        : QObject(parent)
        , _account(account)
        , _syncOptions(options)
        , _baseUrl(baseUrl)
    {
    }
    AccountPtr _account;
    const SyncOptions _syncOptions;
    const QUrl _baseUrl;
    QString _localDir; // absolute path to the local directory. ends with '/'
    QString _remoteFolder; // remote folder, ends with '/'
    SyncJournalDb *_statedb;
    ExcludedFiles *_excludes;
    QRegExp _invalidFilenameRx; // FIXME: maybe move in ExcludedFiles
    QStringList _serverBlacklistedFiles; // The blacklist from the capabilities
    bool _ignoreHiddenFiles = false;
    std::function<bool(const QString &)> _shouldDiscoverLocaly;

    void startJob(ProcessDirectoryJob *);

    void setSelectiveSyncBlackList(const QSet<QString> &list);
    void setSelectiveSyncWhiteList(const QSet<QString> &list);

    // output
    QByteArray _dataFingerprint;
    bool _anotherSyncNeeded = false;

    /**
     * @return whether we are syncing to a ocis space or not
     */
    bool isSpace() const;

signals:
    void fatalError(const QString &errorString);
    void itemDiscovered(const SyncFileItemPtr &item);
    void finished();

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);

    /** For excluded items that don't show up in itemDiscovered()
      *
      * The path is relative to the sync folder, similar to item->_file
      */
    void silentlyExcluded(const QString &folderPath);
    void excluded(const QString &folderPath, CSYNC_EXCLUDE_TYPE reason);
};

/// Implementation of DiscoveryPhase::adjustRenamedPath
QString adjustRenamedPath(const QHash<QString, QString> &renamedItems, const QString &original);
}
