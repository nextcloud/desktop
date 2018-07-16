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

class Account;
class SyncJournalDb;

/**
 * The Discovery Phase was once called "update" phase in csync terms.
 * Its goal is to look at the files in one of the remote and check compared to the db
 * if the files are new, or changed.
 */

struct DiscoveryDirectoryResult
{
    QString path;
    QString msg;
    int code;
    std::deque<std::unique_ptr<csync_file_stat_t>> list;
    DiscoveryDirectoryResult()
        : code(EIO)
    {
    }
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
    std::deque<std::unique_ptr<csync_file_stat_t>> &&takeResults() { return std::move(_results); }

    // This is not actually a network job, it is just a job
signals:
    void firstDirectoryPermissions(RemotePermissions);
    void etagConcatenation(const QString &);
    void etag(const QString &);
    void finishedWithResult();
    void finishedWithError(int csyncErrnoCode, const QString &msg);
private slots:
    void directoryListingIteratedSlot(QString, const QMap<QString, QString> &);
    void lsJobFinishedWithoutErrorSlot();
    void lsJobFinishedWithErrorSlot(QNetworkReply *);

private:
    std::deque<std::unique_ptr<csync_file_stat_t>> _results;
    QString _subPath;
    QString _etagConcatenation;
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
    bool _ignoreHiddenFiles = false;

    bool isInSelectiveSyncBlackList(const QString &path) const;
    bool checkSelectiveSyncNewFolder(const QString &path, RemotePermissions rp);

    QMap<QString, SyncFileItemPtr> _deletedItem;
    QMap<QString, QPointer<QObject>> _queuedDeletedDirectories;
    QMap<QString, QString> _renamedItems; // map source -> destinations
    QString adjustRenamedPath(const QString &original) const;

signals:
    void fatalError(const QString &errorString);
    void folderDiscovered(bool local, QString folderUrl);

    // A new folder was discovered and was not synced because of the confirmation feature
    void newBigFolder(const QString &folder, bool isExternal);
};
}
