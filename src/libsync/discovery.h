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

enum ErrorTag { Error };

template <typename T>
class Result
{
    union {
        T _result;
        QString _errorString;
    };
    bool _isError;

public:
    Result(T value)
        : _result(std::move(value))
        , _isError(false){};
    Result(ErrorTag, QString str)
        : _errorString(std::move(str))
        , _isError(true)
    {
    }
    ~Result()
    {
        if (_isError)
            _errorString.~QString();
        else
            _result.~T();
    }
    explicit operator bool() const { return !_isError; }
    const T &operator*() const &
    {
        ASSERT(!_isError);
        return _result;
    }
    T operator*() &&
    {
        ASSERT(!_isError);
        return std::move(_result);
    }
    QString errorMessage() const
    {
        ASSERT(_isError);
        return _errorString;
    }
};

struct RemoteInfo
{
    QString name;
    QByteArray etag;
    QByteArray fileId;
    QByteArray checksumHeader;
    OCC::RemotePermissions remotePerm;
    time_t modtime = 0;
    int64_t size = 0;
    bool isDirectory = false;
    bool isValid() const { return !name.isNull(); }
};

struct LocalInfo
{
    QString name;
    time_t modtime = 0;
    int64_t size = 0;
    uint64_t inode = 0;
    bool isDirectory = false;
    bool isHidden = false;
    bool isValid() const { return !name.isNull(); }
};

/**
 * Do the propfind on the server.
 * TODO: merge with DiscoverySingleDirectoryJob
 */
class DiscoverServerJob : public DiscoverySingleDirectoryJob
{
    Q_OBJECT
public:
    explicit DiscoverServerJob(const AccountPtr &account, const QString &path, QObject *parent = 0);
signals:
    void finished(const Result<QVector<RemoteInfo>> &result);
};

class ProcessDirectoryJob : public QObject
{
    Q_OBJECT
public:
    enum QueryMode { NormalQuery,
        ParentDontExist,
        ParentNotChanged,
        InBlackList };
    explicit ProcessDirectoryJob(const SyncFileItemPtr &dirItem, QueryMode queryServer, QueryMode queryLocal,
        const QSharedPointer<const DiscoveryPhase> &data, QObject *parent)
        : QObject(parent)
        , _dirItem(dirItem)
        , _queryServer(queryServer)
        , _queryLocal(queryLocal)
        , _discoveryData(data)
        , _currentFolder(dirItem ? dirItem->_file : QString())
    {
    }
    void start();
    void abort();

private:
    void process();
    // return true if the file is excluded
    bool handleExcluded(const QString &path, bool isDirectory, bool isHidden);
    void processFile(const QString &, const LocalInfo &, const RemoteInfo &, const SyncJournalFileRecord &);
    void processBlacklisted(const QString &path, const LocalInfo &, const SyncJournalFileRecord &);
    void subJobFinished();
    void progress();

    QVector<RemoteInfo> _serverEntries;
    QVector<LocalInfo> _localEntries;
    bool _hasServerEntries = false;
    bool _hasLocalEntries = false;
    QPointer<DiscoverServerJob> _serverJob;
    //QScopedPointer<DiscoverLocalJob> _localJob;
    std::deque<ProcessDirectoryJob *> _queuedJobs;
    QVector<ProcessDirectoryJob *> _runningJobs;
    SyncFileItemPtr _dirItem;
    QueryMode _queryServer;
    QueryMode _queryLocal;
    QSharedPointer<const DiscoveryPhase> _discoveryData;
    QString _currentFolder;
    bool _childModified = false; // the directory contains modified item what would prevent deletion
    bool _childIgnored = false; // The directory contains ignored item that would prevent deletion

signals:
    void itemDiscovered(const SyncFileItemPtr &item);
    void finished();
};
}
