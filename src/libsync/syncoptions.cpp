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
#include "common/utility.h"

#include <QRegularExpression>

using namespace OCC;

SyncOptions::SyncOptions(QSharedPointer<Vfs> vfs)
    : _vfs(vfs)
{
}

SyncOptions::~SyncOptions()
{
}

void SyncOptions::fillFromEnvironmentVariables()
{
    QByteArray chunkSizeEnv = qgetenv("OWNCLOUD_CHUNK_SIZE");
    if (!chunkSizeEnv.isEmpty())
        _initialChunkSize = chunkSizeEnv.toUInt();

    QByteArray minChunkSizeEnv = qgetenv("OWNCLOUD_MIN_CHUNK_SIZE");
    if (!minChunkSizeEnv.isEmpty())
        _minChunkSize = minChunkSizeEnv.toUInt();

    QByteArray maxChunkSizeEnv = qgetenv("OWNCLOUD_MAX_CHUNK_SIZE");
    if (!maxChunkSizeEnv.isEmpty())
        _maxChunkSize = maxChunkSizeEnv.toUInt();

    QByteArray targetChunkUploadDurationEnv = qgetenv("OWNCLOUD_TARGET_CHUNK_UPLOAD_DURATION");
    if (!targetChunkUploadDurationEnv.isEmpty())
        _targetChunkUploadDuration = std::chrono::milliseconds(targetChunkUploadDurationEnv.toUInt());

    int maxParallel = qEnvironmentVariableIntValue("OWNCLOUD_MAX_PARALLEL");
    if (maxParallel > 0)
        _parallelNetworkJobs = maxParallel;
}

void SyncOptions::verifyChunkSizes()
{
    _minChunkSize = qMin(_minChunkSize, _initialChunkSize);
    _maxChunkSize = qMax(_maxChunkSize, _initialChunkSize);
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
