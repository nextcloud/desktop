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
#include "discoveryphase.h"
#include "syncfileitem.h"
#include "common/asserts.h"
#include "common/syncjournaldb.h"

class ExcludedFiles;

namespace OCC {
class SyncJournalDb;

/**
 * Job that handles discovery of a directory.
 *
 * This includes:
 *  - Do a DiscoverySingleDirectoryJob network job which will do a PROPFIND of this directory
 *  - Stat all the entries in the local file system for this directory
 *  - Merge all information (and the information from the database) in order to know what needs
 *    to be done for every file within this directory.
 *  - For every sub-directory within this directory, "recursively" create a new ProcessDirectoryJob.
 *
 * This job is tightly coupled with the DiscoveryPhase class.
 *
 * After being start()'ed this job will perform work asynchronously and emit finished() when done.
 *
 * Internally, this job will call DiscoveryPhase::scheduleMoreJobs when one of its sub-jobs is
 * finished. DiscoveryPhase::scheduleMoreJobs will call processSubJobs() to continue work until
 * the job is finished.
 *
 * Results are fed outwards via the DiscoveryPhase::itemDiscovered() signal.
 */
class ProcessDirectoryJob : public QObject
{
    Q_OBJECT

    struct PathTuple;
public:
    enum QueryMode {
        NormalQuery,
        ParentDontExist, // Do not query this folder because it does not exist
        ParentNotChanged, // No need to query this folder because it has not changed from what is in the DB
        InBlackList // Do not query this folder because it is in the blacklist (remote entries only)
    };
    Q_ENUM(QueryMode)

    /** For creating the root job
     *
     * The base pin state is used if the root dir's pin state can't be retrieved.
     */
    explicit ProcessDirectoryJob(DiscoveryPhase *data, PinState basePinState, QObject *parent)
        : QObject(parent)
        , _discoveryData(data)
    {
        computePinState(basePinState);
    }

    /// For creating subjobs
    explicit ProcessDirectoryJob(const PathTuple &path, const SyncFileItemPtr &dirItem,
        QueryMode queryLocal, QueryMode queryServer,
        ProcessDirectoryJob *parent)
        : QObject(parent)
        , _dirItem(dirItem)
        , _queryServer(queryServer)
        , _queryLocal(queryLocal)
        , _discoveryData(parent->_discoveryData)
        , _currentFolder(path)
    {
        computePinState(parent->_pinState);
    }

    void start();
    /** Start up to nbJobs, return the number of job started; emit finished() when done */
    int processSubJobs(int nbJobs);

    SyncFileItemPtr _dirItem;

private:

    /** Structure representing a path during discovery. A same path may have different value locally
     * or on the server in case of renames.
     *
     * These strings never start or ends with slashes. They are all relative to the folder's root.
     * Usually they are all the same and are even shared instance of the same QString.
     *
     * _server and _local paths will differ if there are renames, example:
     *   remote renamed A/ to B/ and local renamed A/X to A/Y then
     *     target:   B/Y/file
     *     original: A/X/file
     *     local:    A/Y/file
     *     server:   B/X/file
     */
    struct PathTuple
    {
        QString _original; // Path as in the DB (before the sync)
        QString _target; // Path that will be the result after the sync (and will be in the DB)
        QString _server; // Path on the server (before the sync)
        QString _local; // Path locally (before the sync)
        static QString pathAppend(const QString &base, const QString &name)
        {
            return base.isEmpty() ? name : base + QLatin1Char('/') + name;
        }
        PathTuple addName(const QString &name) const
        {
            PathTuple result;
            result._original = pathAppend(_original, name);
            auto buildString = [&](const QString &other) {
                // Optimize by trying to keep all string implicitly shared if they are the same (common case)
                return other == _original ? result._original : pathAppend(other, name);
            };
            result._target = buildString(_target);
            result._server = buildString(_server);
            result._local = buildString(_local);
            return result;
        }
    };

    /** Iterate over entries inside the directory (non-recursively).
     *
     * Called once _serverEntries and _localEntries are filled
     * Calls processFile() for each non-excluded one.
     * Will start scheduling subdir jobs when done.
     */
    void process();

    // return true if the file is excluded.
    // path is the full relative path of the file. localName is the base name of the local entry.
    bool handleExcluded(const QString &path, const QString &localName, bool isDirectory,
        bool isHidden, bool isSymlink);

    /** Reconcile local/remote/db information for a single item.
     *
     * Can be a file or a directory.
     * Usually ends up emitting itemDiscovered() or creating a subdirectory job.
     *
     * This main function delegates some work to the processFile* functions.
     */
    void processFile(const PathTuple &, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);

    /// processFile helper for when remote information is available, typically flows into AnalyzeLocalInfo when done
    void processFileAnalyzeRemoteInfo(const SyncFileItemPtr &item, PathTuple, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);

    /// processFile helper for reconciling local changes
    void processFileAnalyzeLocalInfo(const SyncFileItemPtr &item, const PathTuple &, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &, QueryMode recurseQueryServer);

    /// processFile helper for local/remote conflicts
    void processFileConflict(const SyncFileItemPtr &item, const PathTuple &, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);

    /// processFile helper for common final processing
    void processFileFinalize(const SyncFileItemPtr &item, PathTuple, bool recurse, QueryMode recurseQueryLocal, QueryMode recurseQueryServer);


    /** Checks the permission for this item, if needed, change the item to a restoration item.
     * @return false indicate that this is an error and if it is a directory, one should not recurse
     * inside it.
     */
    bool checkPermissions(const SyncFileItemPtr &item);

    struct MovePermissionResult
    {
        // whether moving/renaming the source is ok
        bool sourceOk;
        // whether the destination accepts (always true for renames)
        bool destinationOk;
        // whether creating a new file/dir in the destination is ok
        bool destinationNewOk;
    };

    /**
     * Check if the move is of a specified file within this directory is allowed.
     * Return true if it is allowed, false otherwise
     */
    MovePermissionResult checkMovePermissions(RemotePermissions srcPerm, const QString &srcPath, bool isDirectory);

    void processBlacklisted(const PathTuple &, const LocalInfo &, const SyncJournalFileRecord &dbEntry);
    void subJobFinished();

    /** An DB operation failed */
    void dbError();

    void addVirtualFileSuffix(QString &str) const;
    bool hasVirtualFileSuffix(const QString &str) const;
    Q_REQUIRED_RESULT QString chopVirtualFileSuffix(const QString &str) const;

    /** Convenience to detect suffix-vfs modes */
    bool isVfsWithSuffix() const;

    /** Start a remote discovery network job
     *
     * It fills _serverNormalQueryEntries and sets _serverQueryDone when done.
     */
    DiscoverySingleDirectoryJob *startAsyncServerQuery();

    /** Discover the local directory
      *
      * Fills _localNormalQueryEntries.
      */
    void startAsyncLocalQuery();


    /** Sets _pinState, the directory's pin state
     *
     * If the folder exists locally its state is retrieved, otherwise the
     * parent's pin state is inherited.
     */
    void computePinState(PinState parentState);

    /** Adjust record._type if the db pin state suggests it.
     *
     * If the pin state is stored in the database (suffix vfs only right now)
     * its effects won't be seen in localEntry._type. Instead the effects
     * should materialize in dbEntry._type.
     *
     * This function checks whether the combination of file type and pin
     * state suggests a hydration or dehydration action and changes the
     * _type field accordingly.
     */
    void setupDbPinStateActions(SyncJournalFileRecord &record);

    QueryMode _queryServer = QueryMode::NormalQuery;
    QueryMode _queryLocal = QueryMode::NormalQuery;

    // Holds entries that resulted from a NormalQuery
    QVector<RemoteInfo> _serverNormalQueryEntries;
    QVector<LocalInfo> _localNormalQueryEntries;

    // Whether the local/remote directory item queries are done. Will be set
    // even even for do-nothing (!= NormalQuery) queries.
    bool _serverQueryDone = false;
    bool _localQueryDone = false;

    RemotePermissions _rootPermissions;
    QPointer<DiscoverySingleDirectoryJob> _serverJob;


    /** Number of currently running async jobs.
     *
     * These "async jobs" have nothing to do with the jobs for subdirectories
     * which are being tracked by _queuedJobs and _runningJobs.
     *
     * They are jobs that need to be completed to finish processing of directory
     * entries. This variable is used to ensure this job doesn't finish while
     * these jobs are still in flight.
     */
    int _pendingAsyncJobs = 0;

    /** The queued and running jobs for subdirectories.
     *
     * The jobs are enqueued while processind directory entries and
     * then gradually run via calls to processSubJobs().
     */
    std::deque<ProcessDirectoryJob *> _queuedJobs;
    QVector<ProcessDirectoryJob *> _runningJobs;

    DiscoveryPhase *_discoveryData;

    PathTuple _currentFolder;
    bool _childModified = false; // the directory contains modified item what would prevent deletion
    bool _childIgnored = false; // The directory contains ignored item that would prevent deletion
    PinState _pinState = PinState::Unspecified; // The directory's pin-state, see computePinState()

signals:
    void finished();
    // The root etag of this directory was fetched
    void etag(const QString &, const QDateTime &time);
};
}
