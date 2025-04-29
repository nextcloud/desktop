/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "syncoptions.h"
#include "common/utility.h"

#include <QRegularExpression>

using namespace OCC;

SyncOptions::SyncOptions()
    : _vfs(new VfsOff)
    , _isCmd(false)
{
}

SyncOptions::~SyncOptions() = default;

qint64 SyncOptions::minChunkSize() const
{
    return _minChunkSize;
}

void SyncOptions::setMinChunkSize(const qint64 minChunkSize)
{
    _minChunkSize = ::qBound(_minChunkSize, minChunkSize, _maxChunkSize);
}

qint64 SyncOptions::maxChunkSize() const
{
    return _maxChunkSize;
}

void SyncOptions::setMaxChunkSize(const qint64 maxChunkSize)
{
    _maxChunkSize = ::qBound(_minChunkSize, maxChunkSize, _maxChunkSize);
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

    int maxParallel = qgetenv("OWNCLOUD_MAX_PARALLEL").toInt();
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

void SyncOptions::setIsCmd(const bool isCmd)
{
    _isCmd = isCmd;
}

bool SyncOptions::isCmd() const
{
    return _isCmd;
}
