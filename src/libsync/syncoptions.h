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

    /** Maximum size (in Bytes) a folder can have without asking for confirmation.
     * -1 means infinite */
    qint64 _newBigFolderSizeLimit = -1;

    /** If a confirmation should be asked for external storages */
    bool _confirmExternalStorage = false;

    /** If remotely deleted files are needed to move to trash */
    bool _moveFilesToTrash = false;

    /** Create a virtual file for new files instead of downloading. May not be null */
    QSharedPointer<Vfs> _vfs;

    /** The initial un-adjusted chunk size in bytes for chunked uploads, both
     * for old and new chunking algorithm, which classifies the item to be chunked
     *
     * In chunkingNG, when dynamic chunk size adjustments are done, this is the
     * starting value and is then gradually adjusted within the
     * minChunkSize / maxChunkSize bounds.
     */
    qint64 _initialChunkSize = 10 * 1000 * 1000; // 10MB

    /** The target duration of chunk uploads for dynamic chunk sizing.
     *
     * Set to 0 it will disable dynamic chunk sizing.
     */
    std::chrono::milliseconds _targetChunkUploadDuration = std::chrono::minutes(1);

    /** The maximum number of active jobs in parallel  */
    int _parallelNetworkJobs = 6;

    static constexpr auto chunkV2MinChunkSize = 5LL * 1000LL * 1000LL; // 5 MB
    static constexpr auto chunkV2MaxChunkSize = 5LL * 1000LL * 1000LL * 1000LL; // 5 GB

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

    /** Ensure min <= initial <= max
     *
     * Previously min/max chunk size values didn't exist, so users might
     * have setups where the chunk size exceeds the new min/max default
     * values. To cope with this, adjust min/max to always include the
     * initial chunk size value.
     */
    void verifyChunkSizes();

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

    /** sync had been started via nextcloudcmd command line   */
    [[nodiscard]] bool isCmd() const;
    void setIsCmd(const bool isCmd);

private:
    /**
     * Only sync files that match the expression
     * Invalid pattern by default.
     */
    QRegularExpression _fileRegex = QRegularExpression(QStringLiteral("("));

    qint64 _minChunkSize = chunkV2MinChunkSize;
    qint64 _maxChunkSize = chunkV2MaxChunkSize;

    bool _isCmd = false;
};

}
