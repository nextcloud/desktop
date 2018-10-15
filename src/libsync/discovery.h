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

class ExcludedFiles;

namespace OCC {
class SyncJournalDb;

/**
 * Job that handle the discovering of a directory.
 *
 * This includes:
 *  - Do a DiscoverySingleDirectoryJob network job which will do a PROPFIND of this directory
 *  - Stat all the entries in the local file system for this directory
 *  - Merge all invormation (and the information from the database) in order to know what need
 *    to be done for every file within this directory.
 *  - For every sub-directory within this directory, "recursively" create a new ProcessDirectoryJob
 *
 * This job is tightly couple with the DiscoveryPhase class.
 *
 * After being start()'ed, one must call progress() on this job until it emit finished().
 * This job will call DiscoveryPhase::scheduleMoreJobs when one of its sub-jobs is finished.
 * DiscoveryPhase::scheduleMoreJobs is the one which will call progress().
 */
class ProcessDirectoryJob : public QObject
{
    Q_OBJECT
public:
    enum QueryMode {
        NormalQuery,
        ParentDontExist, // Do not query this folder because it does not exist
        ParentNotChanged, // No need to query this folder because it has not changed from what is in the DB
        InBlackList // Do not query this folder because it is in the blacklist (remote entries only)
    };
    Q_ENUM(QueryMode)
    explicit ProcessDirectoryJob(const SyncFileItemPtr &dirItem, QueryMode queryLocal, QueryMode queryServer,
        DiscoveryPhase *data, QObject *parent)
        : QObject(parent)
        , _dirItem(dirItem)
        , _queryServer(queryServer)
        , _queryLocal(queryLocal)
        , _discoveryData(data)

    {
    }
    void start();
    /** Start up to nbJobs, return the number of job started  */
    int progress(int nbJobs);

    SyncFileItemPtr _dirItem;

private:
    /** Structure representing a path during discovery. A same path may have different value locally
     * or on the server in case of renames.
     *
     * These strings never start or ends with slashes. They are all relative to the folder's root.
     * Usually they are all the same and are even shared instance of the same QString.
     */
    struct PathTuple
    {
        QString _original; // Path as in the DB (before the sync)
        QString _target; // Path that will be the result after the sync (and will be in the DB)
        QString _server; // Path on the server
        QString _local; // Path locally
        PathTuple addName(const QString &name) const
        {
            PathTuple result;
            result._original = _original.isEmpty() ? name : _original + QLatin1Char('/') + name;
            auto buildString = [&](const QString &other) {
                // Optimize by trying to keep all string implicitly shared if they are the same (common case)
                return other == _original ? result._original : other.isEmpty() ? name : other + QLatin1Char('/') + name;
            };
            result._target = buildString(_target);
            result._server = buildString(_server);
            result._local = buildString(_local);
            return result;
        }
    };
    void process();
    // return true if the file is excluded
    bool handleExcluded(const QString &path, bool isDirectory, bool isHidden, bool isSymlink);
    void processFile(PathTuple, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);
    void processFileAnalyzeRemoteInfo(const SyncFileItemPtr &item, PathTuple, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);
    void processFileAnalyzeLocalInfo(const SyncFileItemPtr &item, PathTuple, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &, QueryMode recurseQueryServer);
    void processFileFinalize(const SyncFileItemPtr &item, PathTuple, bool recurse, QueryMode recurseQueryLocal, QueryMode recurseQueryServer);


    /** Checks the permission for this item, if needed, change the item to a restoration item.
     * @return false indicate that this is an error and if it is a directory, one should not recurse
     * inside it.
     */
    bool checkPermissions(const SyncFileItemPtr &item);

    void processBlacklisted(const PathTuple &, const LocalInfo &, const SyncJournalFileRecord &dbEntry);
    void subJobFinished();

    /** An DB operation failed */
    void dbError();

    QVector<RemoteInfo> _serverEntries;
    QVector<LocalInfo> _localEntries;
    RemotePermissions _rootPermissions;
    bool _hasServerEntries = false;
    bool _hasLocalEntries = false;
    int _pendingAsyncJobs = 0;
    QPointer<DiscoverySingleDirectoryJob> _serverJob;
    std::deque<ProcessDirectoryJob *> _queuedJobs;
    QVector<ProcessDirectoryJob *> _runningJobs;
    QueryMode _queryServer;
    QueryMode _queryLocal;
    DiscoveryPhase *_discoveryData;

    PathTuple _currentFolder;
    bool _childModified = false; // the directory contains modified item what would prevent deletion
    bool _childIgnored = false; // The directory contains ignored item that would prevent deletion

signals:
    void finished();
    // The root etag of this directory was fetched
    void etag(const QString &);
};
}
