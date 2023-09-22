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

#include "syncoptions.h"
#include "account.h"
#include "common/utility.h"

#include <QRegularExpression>

using namespace OCC;

SyncOptions::SyncOptions()
    : _vfs(new VfsOff)
{
}

SyncOptions::~SyncOptions() = default;

qint64 SyncOptions::minChunkSize() const
{
    return _minChunkSize;
}

void SyncOptions::setMinChunkSize(const qint64 value)
{
    _minChunkSize = qBound(chunkV2MinChunkSize, value, _maxChunkSize);
    setInitialChunkSize(_initialChunkSize);
}

qint64 SyncOptions::maxChunkSize() const
{
    return _maxChunkSize;
}

void SyncOptions::setMaxChunkSize(const qint64 value)
{
    _maxChunkSize = qBound(_minChunkSize, value, chunkV2MaxChunkSize);
    setInitialChunkSize(_initialChunkSize);
}

qint64 SyncOptions::initialChunkSize() const
{
    return _initialChunkSize;
}

void SyncOptions::setInitialChunkSize(const qint64 value)
{
    _initialChunkSize = toValidChunkSize(value);
}

void SyncOptions::fillFromEnvironmentVariables()
{
    QByteArray minChunkSizeEnv = qgetenv("OWNCLOUD_MIN_CHUNK_SIZE");
    if (!minChunkSizeEnv.isEmpty())
        setMinChunkSize(minChunkSizeEnv.toUInt());

    QByteArray maxChunkSizeEnv = qgetenv("OWNCLOUD_MAX_CHUNK_SIZE");
    if (!maxChunkSizeEnv.isEmpty())
        setMaxChunkSize(maxChunkSizeEnv.toUInt());

    QByteArray chunkSizeEnv = qgetenv("OWNCLOUD_CHUNK_SIZE");
    if (!chunkSizeEnv.isEmpty())
        setInitialChunkSize(chunkSizeEnv.toUInt());

    QByteArray targetChunkUploadDurationEnv = qgetenv("OWNCLOUD_TARGET_CHUNK_UPLOAD_DURATION");
    if (!targetChunkUploadDurationEnv.isEmpty())
        _targetChunkUploadDuration = std::chrono::milliseconds(targetChunkUploadDurationEnv.toUInt());

    int maxParallel = qgetenv("OWNCLOUD_MAX_PARALLEL").toInt();
    if (maxParallel > 0)
        _parallelNetworkJobs = maxParallel;
}

void SyncOptions::fillFromAccount(const AccountPtr account)
{
    if (!account) {
        return;
    }

    if (account->isHttp2Supported() && _parallelNetworkJobs == defaultParallelNetworkJobs) {
        _parallelNetworkJobs = defaultParallelNetworkJobsH2;
    }

    if (const auto size = account->getMaxRequestSize(); size > 0) {
        setMaxChunkSize(size);
    }

    if (account->capabilities().chunkingNg()) {
        // read last used chunk size and use it as initial value
        if (const auto size = account->getLastChunkSize(); size > 0) {
            setInitialChunkSize(size);
        }
    } else {
        // disable dynamic chunk sizing as it is not supported for this account
        _targetChunkUploadDuration = std::chrono::milliseconds(0);
    }
}

qint64 SyncOptions::predictedGoodChunkSize(const qint64 currentChunkSize, const std::chrono::milliseconds uploadTime) const
{
    if (isDynamicChunkSize() && uploadTime.count() > 0) {
        return (currentChunkSize * _targetChunkUploadDuration) / uploadTime;
    }
    return currentChunkSize;
}

QRegularExpression SyncOptions::fileRegex() const
{
    return _fileRegex;
}

void SyncOptions::setFilePattern(const QString &pattern)
{
    // full match or a path ending with this pattern
    setPathPattern(QStringLiteral("(^|/|\\\\)") + pattern + QLatin1Char('$'));
}

void SyncOptions::setPathPattern(const QString &pattern)
{
    _fileRegex.setPatternOptions(Utility::fsCasePreserving() ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
    _fileRegex.setPattern(pattern);
}
