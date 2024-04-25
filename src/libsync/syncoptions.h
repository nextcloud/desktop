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

#include "owncloudlib.h"
#include "common/vfs.h"
#include "common/syncjournaldb.h"

#include <QRegularExpression>
#include <QSharedPointer>
#include <QString>

#include <chrono>


namespace OCC {

/**
 * Value class containing the options given to the sync engine
 */
class OWNCLOUDSYNC_EXPORT SyncOptions
{
public:
    SyncOptions();
    ~SyncOptions();

    /** Min chunk size and the default value */
    static constexpr auto chunkV2MinChunkSize = 5LL * 1000LL * 1000LL; // 5 MB

    /** Max chunk size and the default value */
    static constexpr auto chunkV2MaxChunkSize = 5LL * 1000LL * 1000LL * 1000LL; // 5 GB

    /** Default upload duration target for dynamically sized chunks */
    static constexpr auto chunkV2TargetChunkUploadDuration = std::chrono::minutes(1);

    /** Default initial chunk size for dynamic chunks or the chunk size for fixed size transfers */
    static constexpr auto defaultChunkSize = 10LL * 1000LL * 1000LL; // 10 MB

    /** Max possible chunk size after an upload caused HTTP-413 or RemoteHostClosedError */
    static constexpr auto maxChunkSizeAfterFailure = 100LL * 1000LL * 1000LL; // 100 MB

    /** Default value for parallel network jobs */
    static constexpr auto defaultParallelNetworkJobs = 6;

    /** Default value for parallel network jobs for HTTP/2 connections */
    static constexpr auto defaultParallelNetworkJobsH2 = 20;

    /** Maximum size (in Bytes) a folder can have without asking for confirmation.
     * -1 means infinite */
    qint64 _newBigFolderSizeLimit = -1;

    /** If a confirmation should be asked for external storages */
    bool _confirmExternalStorage = false;

    /** If remotely deleted files are needed to move to trash */
    bool _moveFilesToTrash = false;

    /** Create a virtual file for new files instead of downloading. May not be null */
    QSharedPointer<Vfs> _vfs;

    /** The target duration of chunk uploads for dynamic chunk sizing.
     *
     * Set to 0 it will disable dynamic chunk sizing.
     */
    std::chrono::milliseconds _targetChunkUploadDuration = chunkV2TargetChunkUploadDuration;

    /** The maximum number of active jobs in parallel  */
    int _parallelNetworkJobs = defaultParallelNetworkJobs;

    /** True for dynamic chunk sizing, false if _initialChunkSize should always be used as chunk size */
    bool isDynamicChunkSize() const { return _targetChunkUploadDuration.count() > 0; };
    
    /** Returns a good new chunk size based on the current size and time of the uploaded chunk */
    qint64 predictedGoodChunkSize(const qint64 currentChunkSize, const std::chrono::milliseconds uploadTime) const;
    
    /** Returns chunkSize after applying the configured bounds of _minChunkSize and _maxChunkSize */
    qint64 toValidChunkSize(const qint64 chunkSize) const { return qBound(_minChunkSize, chunkSize, _maxChunkSize); };

    /** The initial chunk size in bytes for chunked uploads */
    [[nodiscard]] qint64 initialChunkSize() const;
    void setInitialChunkSize(const qint64 initialChunkSize);

    /** The minimum chunk size in bytes for chunked uploads */
    [[nodiscard]] qint64 minChunkSize() const;
    void setMinChunkSize(const qint64 minChunkSize);

    /** The maximum chunk size in bytes for chunked uploads */
    [[nodiscard]] qint64 maxChunkSize() const;
    void setMaxChunkSize(const qint64 maxChunkSize);

    /** Reads settings from env vars where available.
     *
     * Currently reads _initialChunkSize, _minChunkSize, _maxChunkSize,
     * _targetChunkUploadDuration, _parallelNetworkJobs.
     */
    void fillFromEnvironmentVariables();

    /** Reads settings from the given account when available.
     * Currently reads _initialChunkSize and _maxChunkSize.
     */
    void fillFromAccount(const AccountPtr account);

    /** A regular expression to match file names
     * If no pattern is provided the default is an invalid regular expression.
     */
    [[nodiscard]] QRegularExpression fileRegex() const;

    /**
     * A pattern like *.txt, matching only file names
     */
    void setFilePattern(const QString &pattern);

    /**
     * A pattern like /own.*\/.*txt matching the full path
     */
    void setPathPattern(const QString &pattern);

private:
    /**
     * Only sync files that match the expression
     * Invalid pattern by default.
     */
    QRegularExpression _fileRegex = QRegularExpression(QStringLiteral("("));

    qint64 _initialChunkSize = defaultChunkSize;
    qint64 _minChunkSize = chunkV2MinChunkSize;
    qint64 _maxChunkSize = chunkV2MaxChunkSize;
};

}
