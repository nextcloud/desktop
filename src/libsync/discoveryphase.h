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
#include <QLinkedList>
#include <deque>
#include "syncoptions.h"
#include "syncfileitem.h"

class ExcludedFiles;

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
    QByteArray etag;
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
    bool isDirectory = false;
    bool isHidden = false;
    bool isVirtualFile = false;
    bool isSymLink = false;
    bool isValid() const { return !name.isNull(); }
};


/**
 * @brief The DiscoverySingleDirectoryJob class
 *
 * Run in the main thread, reporting to the DiscoveryJobMainThread object
 *
 * @ingroup libsync
 */
class DiscoverySingleDirectoryJob : public QObject
{
    Q_OBJECT
public:
    explicit DiscoverySingleDirectoryJob(const AccountPtr &account, const QString &path, QObject *parent = 0);
    // Specify thgat this is the root and we need to check the data-fingerprint
    void setIsRootPath() { _isRootPath = true; }
    void start();
    void abort();

    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(RemotePermissions);
    void etag(const QString &);
    void finished(const Result<QVector<RemoteInfo>> &result);
private slots:
    void directoryListingIteratedSlot(QString, const QMap<QString, QString> &);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply *);

private:
    QVector<RemoteInfo> _results;
    QString _subPath;
    QString _firstEtag;
    AccountPtr _account;
    // The first result is for the directory itself and need to be ignored.
    // This flag is true if it was already ignored.
    bool _ignoredFirst;
    // Set to true if this is the root path and we need to check the data-fingerprint
    bool _isRootPath;
    // If this directory is an external storage (The first item has 'M' in its permission)
    bool _isExternalStorage;
    // If set, the discovery will finish with an error
    QString _error;
    QPointer<LsColJob> _lsColJob;

public:
    QByteArray _dataFingerprint;
};

class DiscoveryPhase : public QObject
{
    Q_OBJECT

    ProcessDirectoryJob *_currentRootJob = nullptr;

public:
    QString _localDir; // absolute path to the local directory. ends with '/'
    QString _remoteFolder; // remote folder, ends with '/'
    SyncJournalDb *_statedb;
    AccountPtr _account;
    SyncOptions _syncOptions;
    QStringList _selectiveSyncBlackList;
    QStringList _selectiveSyncWhiteList;
    ExcludedFiles *_excludes;
    QString _invalidFilenamePattern; // FIXME: maybe move in ExcludedFiles
    int _currentlyActiveJobs = 0;
    bool _ignoreHiddenFiles = false;
    std::function<bool(const QString &)> _shouldDiscoverLocaly;

    bool isInSelectiveSyncBlackList(const QString &path) const;

    // Check if the new folder should be deselected or not.
    // May be async. "Return" via the callback, true if the item is blacklisted
    void checkSelectiveSyncNewFolder(const QString &path, RemotePermissions rp,
        std::function<void(bool)> callback);

    QMap<QString, SyncFileItemPtr> _deletedItem;
    QMap<QString, ProcessDirectoryJob *> _queuedDeletedDirectories;
    QMap<QString, QString> _renamedItems; // map source -> destinations
    /** Given an original path, return the target path obtained when renaming is done.
     *
     * Note that it only considers parent directory renames. So if A/B got renamed to C/D,
     * checking A/B/file would yield C/D/file, but checking A/B would yield A/B.
     */
    QString adjustRenamedPath(const QString &original) const;

    void startJob(ProcessDirectoryJob *);

    QByteArray _dataFingerprint;

    void scheduleMoreJobs();
signals:
    void fatalError(const QString &errorString);
    void itemDiscovered(const SyncFileItemPtr &item);
    void finished();

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);
};
}
