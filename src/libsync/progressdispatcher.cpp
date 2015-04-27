/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "progressdispatcher.h"

#include <QObject>
#include <QMetaType>
#include <QDebug>
#include <QCoreApplication>

namespace OCC {

ProgressDispatcher* ProgressDispatcher::_instance = 0;

QString Progress::asResultString( const SyncFileItem& item)
{
    switch(item._instruction) {
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
        if (item._direction != SyncFileItem::Up) {
            return QCoreApplication::translate( "progress", "Downloaded");
        } else {
            return QCoreApplication::translate( "progress", "Uploaded");
        }
    case CSYNC_INSTRUCTION_CONFLICT:
        return QCoreApplication::translate( "progress", "Downloaded, renamed conflicting file");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate( "progress", "Deleted");
    case CSYNC_INSTRUCTION_EVAL_RENAME:
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate( "progress", "Moved to %1").arg(item._renameTarget);
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate( "progress", "Ignored");
    case CSYNC_INSTRUCTION_STAT_ERROR:
        return QCoreApplication::translate( "progress", "Filesystem access error");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate( "progress", "Error");
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_EVAL:
        return QCoreApplication::translate( "progress", "Unknown");

    }
    return QCoreApplication::translate( "progress", "Unknown");
}

QString Progress::asActionString( const SyncFileItem &item )
{
    switch(item._instruction) {
    case CSYNC_INSTRUCTION_CONFLICT:
    case CSYNC_INSTRUCTION_SYNC:
    case CSYNC_INSTRUCTION_NEW:
        if (item._direction != SyncFileItem::Up)
            return QCoreApplication::translate( "progress", "downloading");
        else
            return QCoreApplication::translate( "progress", "uploading");
    case CSYNC_INSTRUCTION_REMOVE:
        return QCoreApplication::translate( "progress", "deleting");
    case CSYNC_INSTRUCTION_EVAL_RENAME:
    case CSYNC_INSTRUCTION_RENAME:
        return QCoreApplication::translate( "progress", "moving");
    case CSYNC_INSTRUCTION_IGNORE:
        return QCoreApplication::translate( "progress", "ignoring");
    case CSYNC_INSTRUCTION_STAT_ERROR:
        return QCoreApplication::translate( "progress", "error");
    case CSYNC_INSTRUCTION_ERROR:
        return QCoreApplication::translate( "progress", "error");
    case CSYNC_INSTRUCTION_NONE:
    case CSYNC_INSTRUCTION_EVAL:
        break;
    }
    return QString();
}

bool Progress::isWarningKind( SyncFileItem::Status kind)
{
    return  kind == SyncFileItem::SoftError || kind == SyncFileItem::NormalError
         || kind == SyncFileItem::FatalError || kind == SyncFileItem::FileIgnored
         || kind == SyncFileItem::Conflict || kind == SyncFileItem::Restoration;

}

bool Progress::isIgnoredKind( SyncFileItem::Status kind)
{
    return  kind == SyncFileItem::FileIgnored;

}

ProgressDispatcher* ProgressDispatcher::instance() {
    if (!_instance) {
        _instance = new ProgressDispatcher();
    }
    return _instance;
}

ProgressDispatcher::ProgressDispatcher(QObject *parent) :
    QObject(parent)
{

}

ProgressDispatcher::~ProgressDispatcher()
{

}

void ProgressDispatcher::setProgressInfo(const QString& folder, const ProgressInfo& progress)
{
    if( folder.isEmpty())
// The update phase now also has progress
//            (progress._currentItems.size() == 0
//             && progress._totalFileCount == 0) )
    {
        return;
    }
    emit progressInfo( folder, progress );
}

void ProgressInfo::start()
{
    connect(&_updateEstimatesTimer, SIGNAL(timeout()), SLOT(updateEstimates()));
    _updateEstimatesTimer.start(1000);
}

bool ProgressInfo::hasStarted() const
{
    return _updateEstimatesTimer.isActive();
}

void ProgressInfo::adjustTotalsForFile(const SyncFileItem &item)
{
    if (!item._isDirectory) {
        _fileProgress._total++;
        if (isSizeDependent(item)) {
            _sizeProgress._total += item._size;
        }
    } else if (item._instruction != CSYNC_INSTRUCTION_NONE) {
        // Added or removed directories certainly count.
        _fileProgress._total++;
    }
}

void ProgressInfo::adjustTotalSize(qint64 change)
{
    _sizeProgress._total += change;
}

quint64 ProgressInfo::totalFiles() const
{
    return _fileProgress._total;
}

quint64 ProgressInfo::completedFiles() const
{
    return _fileProgress._completed;
}

quint64 ProgressInfo::currentFile() const
{
    return completedFiles() + _currentItems.size();
}

quint64 ProgressInfo::totalSize() const
{
    return _sizeProgress._total;
}

quint64 ProgressInfo::completedSize() const
{
    return _sizeProgress._completed;
}

void ProgressInfo::setProgressComplete(const SyncFileItem &item)
{
    _currentItems.remove(item._file);
    _fileProgress._completed += item._affectedItems;
    if (ProgressInfo::isSizeDependent(item)) {
        _totalSizeOfCompletedJobs += item._size;
    }
    recomputeCompletedSize();
    _lastCompletedItem = item;
}

void ProgressInfo::setProgressItem(const SyncFileItem &item, quint64 size)
{
    _currentItems[item._file]._item = item;
    _currentItems[item._file]._progress._completed = size;
    _currentItems[item._file]._progress._total = item._size;
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
    // So, if we detect a high rate of files per second, we gradually prefer
    // a file-per-second estimate and assume the remaining transfer will
    // be done with the highest speed we've seen.
    quint64 combinedEta = file.estimatedEta + _sizeProgress.remaining() / _maxBytesPerSecond * 1000;
    if (combinedEta < size.estimatedEta) {
        double filesPerSec = _fileProgress._progressPerSec;
        // value between 0 (fps==5) and 1 (fps==20)
        double scale = qBound(0.0, (filesPerSec - 5.0) / 15.0, 1.0);
        size.estimatedEta = (1.0 - scale) * size.estimatedEta + scale * combinedEta;
    }
    return size;
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
    quint64 r = _totalSizeOfCompletedJobs;
    foreach(const ProgressItem &i, _currentItems) {
        if (isSizeDependent(i._item))
            r += i._progress._completed;
    }
    _sizeProgress._completed = r;
}

ProgressInfo::Estimates ProgressInfo::Progress::estimates() const
{
    Estimates est;
    est.estimatedBandwidth = _progressPerSec;
    if (_progressPerSec != 0) {
        est.estimatedEta = (_total - _completed) / _progressPerSec * 1000.0;
    } else {
        est.estimatedEta = 0; // looks better than quint64 max
    }
    return est;
}

quint64 ProgressInfo::Progress::completed() const
{
    return _completed;
}

quint64 ProgressInfo::Progress::remaining() const
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


}
