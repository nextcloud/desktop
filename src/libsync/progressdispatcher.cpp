/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "progressdispatcher.h"

#include <QObject>
#include <QMetaType>
#include <QCoreApplication>

namespace OCC {

ProgressDispatcher *ProgressDispatcher::_instance = nullptr;

QString Progress::asResultString(const SyncFileItem &item)
{
    switch (item._instruction) {
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
        if (item._direction != SyncFileItem::Up) {
            if (item._type == ItemTypeVirtualFile) {
                return QCoreApplication::translate("progress", "Virtual file created");
            } else if (item._type == ItemTypeVirtualFileDehydration) {
                return QCoreApplication::translate("progress", "Replaced by virtual file");
            } else {
                return QCoreApplication::translate("progress", "Downloaded");
            }
        } else {
            return QCoreApplication::translate("progress", "Uploaded");
        }
    case CSYNC_INSTRUCTION_CONFLICT:
        return QCoreApplication::translate("progress", "Server version downloaded, local copy was backed up as conflict file");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate("progress", "Deleted");
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate("progress", "%1 moved to %2").arg(item._file, item._renameTarget);
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate("progress", "Ignored");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate("progress", "Error");
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        return QCoreApplication::translate("progress", "Updated local metadata");
    case CSYNC_INSTRUCTION_NONE:
        return QCoreApplication::translate("progress", "Unknown");
    }
    return QCoreApplication::translate("progress", "Unknown");
}

QString Progress::asActionString(const SyncFileItem &item)
{
    switch (item._instruction) {
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
    case CSYNC_INSTRUCTION_TYPE_CHANGE:
        if (item._direction != SyncFileItem::Up)
            return QCoreApplication::translate("progress", "downloading");
        else
            return QCoreApplication::translate("progress", "uploading");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate("progress", "deleting");
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate("progress", "moving");
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate("progress", "ignoring");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate("progress", "error");
    case CSYNC_INSTRUCTION_UPDATE_METADATA:
        return QCoreApplication::translate("progress", "updating local metadata");
    case CSYNC_INSTRUCTION_NONE:
        break;
    }
    return QString();
}

bool Progress::isWarningKind(SyncFileItem::Status kind)
{
    return kind == SyncFileItem::SoftError || kind == SyncFileItem::NormalError
        || kind == SyncFileItem::FatalError || kind == SyncFileItem::FileIgnored
        || kind == SyncFileItem::Conflict || kind == SyncFileItem::Restoration
        || kind == SyncFileItem::DetailError || kind == SyncFileItem::BlacklistedError;
}

bool Progress::isIgnoredKind(SyncFileItem::Status kind)
{
    return kind == SyncFileItem::FileIgnored;
}

ProgressDispatcher *ProgressDispatcher::instance()
{
    if (!_instance) {
        _instance = new ProgressDispatcher();
    }
    return _instance;
}

ProgressDispatcher::ProgressDispatcher(QObject *parent)
    : QObject(parent)
{
}

ProgressDispatcher::~ProgressDispatcher()
{
}

ProgressInfo::ProgressInfo()
{
    connect(&_updateEstimatesTimer, &QTimer::timeout, this, &ProgressInfo::updateEstimates);
    reset();
}

void ProgressInfo::reset()
{
    _status = None;

    _currentItems.clear();
    _currentDiscoveredRemoteFolder.clear();
    _currentDiscoveredLocalFolder.clear();
    _sizeProgress = Progress();
    _fileProgress = Progress();
    _totalSizeOfCompletedJobs = 0;

    // Historically, these starting estimates were way lower, but that lead
    // to gross overestimation of ETA when a good estimate wasn't available.
    _maxBytesPerSecond = 2000000.0; // 2 MB/s
    _maxFilesPerSecond = 10.0;

    _updateEstimatesTimer.stop();
    _lastCompletedItem = SyncFileItem();
}

ProgressInfo::Status ProgressInfo::status() const
{
    return _status;
}

void ProgressInfo::startEstimateUpdates()
{
    _updateEstimatesTimer.start(1000);
}

bool ProgressInfo::isUpdatingEstimates() const
{
    return _updateEstimatesTimer.isActive();
}

static bool shouldCountProgress(const SyncFileItem &item)
{
    const auto instruction = item._instruction;

    // Skip any ignored, error or non-propagated files and directories.
    if (instruction == CSYNC_INSTRUCTION_NONE
        || instruction == CSYNC_INSTRUCTION_UPDATE_METADATA
        || instruction == CSYNC_INSTRUCTION_IGNORE
        || instruction == CSYNC_INSTRUCTION_ERROR) {
        return false;
    }

    return true;
}

void ProgressInfo::adjustTotalsForFile(const SyncFileItem &item)
{
    if (!shouldCountProgress(item)) {
        return;
    }

    _fileProgress._total += item._affectedItems;
    if (isSizeDependent(item)) {
        _sizeProgress._total += item._size;
    }
}

void ProgressInfo::updateTotalsForFile(const SyncFileItem &item, qint64 newSize)
{
    if (!shouldCountProgress(item)) {
        return;
    }

    if (!_currentItems.contains(item._file)) {
        _sizeProgress._total += newSize - item._size;
    } else {
        _sizeProgress._total += newSize - _currentItems[item._file]._progress._total;
    }

    setProgressItem(item, 0);
    _currentItems[item._file]._progress._total = newSize;
}

qint64 ProgressInfo::totalFiles() const
{
    return _fileProgress._total;
}

qint64 ProgressInfo::completedFiles() const
{
    return _fileProgress._completed;
}

qint64 ProgressInfo::currentFile() const
{
    return completedFiles() + _currentItems.size();
}

qint64 ProgressInfo::totalSize() const
{
    return _sizeProgress._total;
}

qint64 ProgressInfo::completedSize() const
{
    return _sizeProgress._completed;
}

void ProgressInfo::setProgressComplete(const SyncFileItem &item)
{
    if (!shouldCountProgress(item)) {
        return;
    }

    _fileProgress.setCompleted(_fileProgress._completed + item._affectedItems);
    if (ProgressInfo::isSizeDependent(item)) {
        _totalSizeOfCompletedJobs += _currentItems[item._file]._progress._total;
    }
    _currentItems.remove(item._file);
    recomputeCompletedSize();
    _lastCompletedItem = item;
}

void ProgressInfo::setProgressItem(const SyncFileItem &item, qint64 completed)
{
    if (!shouldCountProgress(item)) {
        return;
    }

    if (!_currentItems.contains(item._file)) {
        _currentItems[item._file]._item = item;
        _currentItems[item._file]._progress._total = item._size;
    }
    _currentItems[item._file]._progress.setCompleted(completed);
    recomputeCompletedSize();

    // This seems dubious!
    _lastCompletedItem = SyncFileItem();
}

ProgressInfo::Estimates ProgressInfo::totalProgress() const
{
    Estimates file = _fileProgress.estimates();
    if (_sizeProgress._total == 0) {
        return file;
    }

    Estimates size = _sizeProgress.estimates();

    // Ideally the remaining time would be modeled as:
    //   remaning_file_sizes / transfer_speed
    //   + remaining_file_count * per_file_overhead
    //   + remaining_chunked_file_sizes / chunked_reassembly_speed
    // with us estimating the three parameters in conjunction.
    //
    // But we currently only model the bandwidth and the files per
    // second independently, which leads to incorrect values. To slightly
    // mitigate this problem, we combine the two models depending on
    // which factor dominates (essentially big-file-upload vs.
    // many-small-files)
    //
    // If we have size information, we prefer an estimate based
    // on the upload speed. That's particularly relevant for large file
    // up/downloads, where files per second will be close to 0.
    //
    // However, when many *small* files are transfered, the estimate
    // can become very pessimistic as the transfered amount per second
    // drops significantly.
    //
    // So, if we detect a high rate of files per second or a very low
    // transfer rate (often drops hugely during a sequence of deletes,
    // for instance), we gradually prefer an optimistic estimate and
    // assume the remaining transfer will be done with the highest speed
    // we've seen.

    // Compute a value that is 0 when fps is <=L*max and 1 when fps is >=U*max
    double fps = _fileProgress._progressPerSec;
    double fpsL = 0.5;
    double fpsU = 0.8;
    double nearMaxFps =
        qBound(0.0,
            (fps - fpsL * _maxFilesPerSecond) / ((fpsU - fpsL) * _maxFilesPerSecond),
            1.0);

    // Compute a value that is 0 when transfer is >= U*max and
    // 1 when transfer is <= L*max
    double trans = _sizeProgress._progressPerSec;
    double transU = 0.1;
    double transL = 0.01;
    double slowTransfer = 1.0 - qBound(0.0,
                                    (trans - transL * _maxBytesPerSecond) / ((transU - transL) * _maxBytesPerSecond),
                                    1.0);

    double beOptimistic = nearMaxFps * slowTransfer;
    size.estimatedEta = quint64((1.0 - beOptimistic) * size.estimatedEta
        + beOptimistic * optimisticEta());

    return size;
}

quint64 ProgressInfo::optimisticEta() const
{
    // This assumes files and transfers finish as quickly as possible
    // *but* note that maxPerSecond could be serious underestimate
    // (if we never got to fully excercise transfer or files/second)

    return _fileProgress.remaining() / _maxFilesPerSecond * 1000
        + _sizeProgress.remaining() / _maxBytesPerSecond * 1000;
}

bool ProgressInfo::trustEta() const
{
    return totalProgress().estimatedEta < 100 * optimisticEta();
}

ProgressInfo::Estimates ProgressInfo::fileProgress(const SyncFileItem &item) const
{
    return _currentItems[item._file]._progress.estimates();
}

void ProgressInfo::updateEstimates()
{
    _sizeProgress.update();
    _fileProgress.update();

    // Update progress of all running items.
    QMutableHashIterator<QString, ProgressItem> it(_currentItems);
    while (it.hasNext()) {
        it.next();
        it.value()._progress.update();
    }

    _maxFilesPerSecond = qMax(_fileProgress._progressPerSec,
        _maxFilesPerSecond);
    _maxBytesPerSecond = qMax(_sizeProgress._progressPerSec,
        _maxBytesPerSecond);
}

void ProgressInfo::recomputeCompletedSize()
{
    qint64 r = _totalSizeOfCompletedJobs;
    for (const auto &i : qAsConst(_currentItems)) {
        if (isSizeDependent(i._item))
            r += i._progress._completed;
    }
    _sizeProgress.setCompleted(r);
}

ProgressInfo::Estimates ProgressInfo::Progress::estimates() const
{
    Estimates est;
    est.estimatedBandwidth = qint64(_progressPerSec);
    if (_progressPerSec != 0.0) {
        est.estimatedEta = quint64((_total - _completed) / _progressPerSec * 1000.0);
    } else {
        est.estimatedEta = 0; // looks better than qint64 max
    }
    return est;
}

qint64 ProgressInfo::Progress::completed() const
{
    return _completed;
}

qint64 ProgressInfo::Progress::remaining() const
{
    return _total - _completed;
}

void ProgressInfo::Progress::update()
{
    // A good way to think about the smoothing factor:
    // If we make progress P per sec and then stop making progress at all,
    // after N calls to this function (and thus seconds) the _progressPerSec
    // will have reduced to P*smoothing^N.
    // With a value of 0.9, only 4% of the original value is left after 30s
    //
    // In the first few updates we want to go to the correct value quickly.
    // Therefore, smoothing starts at 0 and ramps up to its final value over time.
    const double smoothing = 0.9 * (1.0 - _initialSmoothing);
    _initialSmoothing *= 0.7; // goes from 1 to 0.03 in 10s
    _progressPerSec = smoothing * _progressPerSec + (1.0 - smoothing) * (_completed - _prevCompleted);
    _prevCompleted = _completed;
}

void ProgressInfo::Progress::setCompleted(qint64 completed)
{
    _completed = qMin(completed, _total);
    _prevCompleted = qMin(_prevCompleted, _completed);
}
}
