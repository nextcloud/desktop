/*
 * Copyright (C) by Markus Goetz <markus@woboq.com>
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

#include "owncloudpropagator.h"
#include "propagatedownload.h"
#include "propagateupload.h"
#include "propagatorjobs.h"
#include "common/utility.h"

#ifdef Q_OS_WIN
#include <windef.h>
#include <winbase.h>
#endif

#include <QLoggingCategory>
#include <QTimer>
#include <QObject>

namespace OCC {

Q_LOGGING_CATEGORY(lcBandwidthManager, "nextcloud.sync.bandwidthmanager", QtInfoMsg)

// Because of the many layers of buffering inside Qt (and probably the OS and the network)
// we cannot lower this value much more. If we do, the estimated bw will be very high
// because the buffers fill fast while the actual network algorithms are not relevant yet.
static qint64 relativeLimitMeasuringTimerIntervalMsec = 1000 * 2;
// See also WritingState in http://code.woboq.org/qt5/qtbase/src/network/access/qhttpprotocolhandler.cpp.html#_ZN20QHttpProtocolHandler11sendRequestEv

// FIXME At some point:
//  * Register device only after the QNR received its metaDataChanged() signal
//  * Incorporate Qt buffer fill state (it's a negative absolute delta).
//  * Incorporate SSL overhead (percentage)
//  * For relative limiting, do less measuring and more delaying+giving quota
//  * For relative limiting, smoothen measurements

BandwidthManager::BandwidthManager(OwncloudPropagator *p)
    : QObject()
    , _propagator(p)
{
    _currentUploadLimit = _propagator->_uploadLimit;
    _currentDownloadLimit = _propagator->_downloadLimit;

    QObject::connect(&_switchingTimer, &QTimer::timeout, this, &BandwidthManager::switchingTimerExpired);
    _switchingTimer.setInterval(10 * 1000);
    _switchingTimer.start();
    QMetaObject::invokeMethod(this, "switchingTimerExpired", Qt::QueuedConnection);

    // absolute uploads/downloads
    QObject::connect(&_absoluteLimitTimer, &QTimer::timeout, this, &BandwidthManager::absoluteLimitTimerExpired);
    _absoluteLimitTimer.setInterval(1000);
    _absoluteLimitTimer.start();

    // Relative uploads
    QObject::connect(&_relativeUploadMeasuringTimer, &QTimer::timeout,
        this, &BandwidthManager::relativeUploadMeasuringTimerExpired);
    _relativeUploadMeasuringTimer.setInterval(relativeLimitMeasuringTimerIntervalMsec);
    _relativeUploadMeasuringTimer.start();
    _relativeUploadMeasuringTimer.setSingleShot(true); // will be restarted from the delay timer
    QObject::connect(&_relativeUploadDelayTimer, &QTimer::timeout,
        this, &BandwidthManager::relativeUploadDelayTimerExpired);
    _relativeUploadDelayTimer.setSingleShot(true); // will be restarted from the measuring timer

    // Relative downloads
    QObject::connect(&_relativeDownloadMeasuringTimer, &QTimer::timeout,
        this, &BandwidthManager::relativeDownloadMeasuringTimerExpired);
    _relativeDownloadMeasuringTimer.setInterval(relativeLimitMeasuringTimerIntervalMsec);
    _relativeDownloadMeasuringTimer.start();
    _relativeDownloadMeasuringTimer.setSingleShot(true); // will be restarted from the delay timer
    QObject::connect(&_relativeDownloadDelayTimer, &QTimer::timeout,
        this, &BandwidthManager::relativeDownloadDelayTimerExpired);
    _relativeDownloadDelayTimer.setSingleShot(true); // will be restarted from the measuring timer
}

BandwidthManager::~BandwidthManager() = default;

void BandwidthManager::registerUploadDevice(UploadDevice *p)
{
    _absoluteUploadDeviceList.push_back(p);
    _relativeUploadDeviceList.push_back(p);
    QObject::connect(p, &QObject::destroyed, this, &BandwidthManager::unregisterUploadDevice);

    if (usingAbsoluteUploadLimit()) {
        p->setBandwidthLimited(true);
        p->setChoked(false);
    } else if (usingRelativeUploadLimit()) {
        p->setBandwidthLimited(true);
        p->setChoked(true);
    } else {
        p->setBandwidthLimited(false);
        p->setChoked(false);
    }
}

void BandwidthManager::unregisterUploadDevice(QObject *o)
{
    auto p = reinterpret_cast<UploadDevice *>(o); // note, we might already be in the ~QObject
    _absoluteUploadDeviceList.remove(p);
    _relativeUploadDeviceList.remove(p);
    if (p == _relativeLimitCurrentMeasuredDevice) {
        _relativeLimitCurrentMeasuredDevice = nullptr;
        _relativeUploadLimitProgressAtMeasuringRestart = 0;
    }
}

void BandwidthManager::registerDownloadJob(GETFileJob *j)
{
    _downloadJobList.push_back(j);
    QObject::connect(j, &QObject::destroyed, this, &BandwidthManager::unregisterDownloadJob);

    if (usingAbsoluteDownloadLimit()) {
        j->setBandwidthLimited(true);
        j->setChoked(false);
    } else if (usingRelativeDownloadLimit()) {
        j->setBandwidthLimited(true);
        j->setChoked(true);
    } else {
        j->setBandwidthLimited(false);
        j->setChoked(false);
    }
}

void BandwidthManager::unregisterDownloadJob(QObject *o)
{
    auto *j = reinterpret_cast<GETFileJob *>(o); // note, we might already be in the ~QObject
    _downloadJobList.remove(j);
    if (_relativeLimitCurrentMeasuredJob == j) {
        _relativeLimitCurrentMeasuredJob = nullptr;
        _relativeDownloadLimitProgressAtMeasuringRestart = 0;
    }
}

void BandwidthManager::relativeUploadMeasuringTimerExpired()
{
    if (!usingRelativeUploadLimit() || _relativeUploadDeviceList.empty()) {
        // Not in this limiting mode, just wait 1 sec to continue the cycle
        _relativeUploadDelayTimer.setInterval(1000);
        _relativeUploadDelayTimer.start();
        return;
    }

    if (!_relativeLimitCurrentMeasuredDevice) {
        qCDebug(lcBandwidthManager) << "No device set, just waiting 1 sec";
        _relativeUploadDelayTimer.setInterval(1000);
        _relativeUploadDelayTimer.start();
        return;
    }

    qCDebug(lcBandwidthManager) << _relativeUploadDeviceList.size() << "Starting Delay";

    const auto currentReadWithProgress = _relativeLimitCurrentMeasuredDevice->_readWithProgress;
    const auto currentRead = _relativeLimitCurrentMeasuredDevice->_read;
    const auto relativeLimitProgressMeasured = (currentReadWithProgress + currentRead) / 2;
    const auto relativeLimitProgressDifference = relativeLimitProgressMeasured - _relativeUploadLimitProgressAtMeasuringRestart;

    qCDebug(lcBandwidthManager) << _relativeUploadLimitProgressAtMeasuringRestart
                                << relativeLimitProgressMeasured
                                << relativeLimitProgressDifference;

    const auto speedkBPerSec = (relativeLimitProgressDifference / relativeLimitMeasuringTimerIntervalMsec * 1000) / 1024;

    qCDebug(lcBandwidthManager) << relativeLimitProgressDifference / 1024 << "kB =>" << speedkBPerSec << "kB/sec on full speed ("
                                << currentReadWithProgress << currentRead
                                << qAbs(currentReadWithProgress - currentRead)
                                << ")";

    const auto uploadLimitPercent = qMax( qMin(-_currentUploadLimit, qint64(90)), qint64(10) ); // Clamp value
    const auto wholeTimeMsec = (100.0 / uploadLimitPercent) * relativeLimitMeasuringTimerIntervalMsec;
    const auto waitTimeMsec = wholeTimeMsec - relativeLimitMeasuringTimerIntervalMsec;
    const auto realWaitTimeMsec = waitTimeMsec + wholeTimeMsec;

    qCDebug(lcBandwidthManager) << waitTimeMsec << " - " << realWaitTimeMsec << " msec for " << uploadLimitPercent << "%";

    // We want to wait twice as long since we want to give all
    // devices the same quota we used now since we don't want
    // any upload to timeout
    _relativeUploadDelayTimer.setInterval(realWaitTimeMsec);
    _relativeUploadDelayTimer.start();

    const auto deviceCount = _relativeUploadDeviceList.size();
    const auto  quotaPerDevice = relativeLimitProgressDifference * (uploadLimitPercent / 100.0) / deviceCount + 1.0;

    for (const auto uploadDevice : _relativeUploadDeviceList) {
        uploadDevice->setBandwidthLimited(true);
        uploadDevice->setChoked(false);
        uploadDevice->giveBandwidthQuota(quotaPerDevice);
        qCDebug(lcBandwidthManager) << "Gave" << quotaPerDevice / 1024.0 << "kB to" << uploadDevice;
    }

    _relativeLimitCurrentMeasuredDevice = nullptr;
}

void BandwidthManager::relativeUploadDelayTimerExpired()
{
    // Switch to measuring state
    _relativeUploadMeasuringTimer.start(); // always start to continue the cycle

    if (!usingRelativeUploadLimit()) {
        return; // oh, not actually needed
    }

    if (_relativeUploadDeviceList.empty()) {
        return;
    }

    qCDebug(lcBandwidthManager) << _relativeUploadDeviceList.size() << "Starting measuring";

    // Take first device and then append it again (= we round robin all devices)
    _relativeLimitCurrentMeasuredDevice = _relativeUploadDeviceList.front();
    _relativeUploadDeviceList.pop_front();
    _relativeUploadDeviceList.push_back(_relativeLimitCurrentMeasuredDevice);

    const auto currentReadWithProgress = _relativeLimitCurrentMeasuredDevice->_readWithProgress;
    const auto currentRead =  _relativeLimitCurrentMeasuredDevice->_read;
    _relativeUploadLimitProgressAtMeasuringRestart = (currentReadWithProgress + currentRead) / 2;

    _relativeLimitCurrentMeasuredDevice->setBandwidthLimited(false);
    _relativeLimitCurrentMeasuredDevice->setChoked(false);

    // choke all other UploadDevices
    for (const auto uploadDevice : _relativeUploadDeviceList) {
        if (uploadDevice == _relativeLimitCurrentMeasuredDevice) {
            continue;
        }

        uploadDevice->setBandwidthLimited(true);
        uploadDevice->setChoked(true);
    }

    // now we're in measuring state
}

// for downloads:
void BandwidthManager::relativeDownloadMeasuringTimerExpired()
{
    if (!usingRelativeDownloadLimit() || _downloadJobList.empty()) {
        // Not in this limiting mode, just wait 1 sec to continue the cycle
        _relativeDownloadDelayTimer.setInterval(1000);
        _relativeDownloadDelayTimer.start();
        return;
    }
    if (!_relativeLimitCurrentMeasuredJob) {
        qCDebug(lcBandwidthManager) << "No job set, just waiting 1 sec";
        _relativeDownloadDelayTimer.setInterval(1000);
        _relativeDownloadDelayTimer.start();
        return;
    }

    qCDebug(lcBandwidthManager) << _downloadJobList.size() << "Starting Delay";

    const auto relativeLimitProgressMeasured = _relativeLimitCurrentMeasuredJob->currentDownloadPosition();
    const auto relativeLimitProgressDifference = relativeLimitProgressMeasured - _relativeDownloadLimitProgressAtMeasuringRestart;

    qCDebug(lcBandwidthManager) << _relativeDownloadLimitProgressAtMeasuringRestart
                                << relativeLimitProgressMeasured << relativeLimitProgressDifference;

    const auto speedkBPerSec = (relativeLimitProgressDifference / relativeLimitMeasuringTimerIntervalMsec * 1000) / 1024;

    qCDebug(lcBandwidthManager) << relativeLimitProgressDifference / 1024 << "kB =>" << speedkBPerSec << "kB/sec on full speed ("
                                << _relativeLimitCurrentMeasuredJob->currentDownloadPosition();

    const auto downloadLimitPercent = qMax( qMin(-_currentDownloadLimit, qint64(90)), qint64(10));
    const auto wholeTimeMsec = (100.0 / downloadLimitPercent) * relativeLimitMeasuringTimerIntervalMsec;
    const auto waitTimeMsec = wholeTimeMsec - relativeLimitMeasuringTimerIntervalMsec;
    const auto realWaitTimeMsec = waitTimeMsec + wholeTimeMsec;

    qCDebug(lcBandwidthManager) << waitTimeMsec << " - " << realWaitTimeMsec << " msec for " << downloadLimitPercent << "%";

    // We want to wait twice as long since we want to give all
    // devices the same quota we used now since we don't want
    // any download to timeout
    _relativeDownloadDelayTimer.setInterval(realWaitTimeMsec);
    _relativeDownloadDelayTimer.start();

    const auto jobCount = _downloadJobList.size();
    auto quota = relativeLimitProgressDifference * (downloadLimitPercent / 100.0);

    if (quota > 20 * 1024) {
        qCDebug(lcBandwidthManager) << "ADJUSTING QUOTA FROM " << quota << " TO " << quota - 20 * 1024;
        quota -= 20 * 1024;
    }

    const auto quotaPerJob = quota / jobCount + 1;
    for (const auto getFileJob : _downloadJobList) {
        getFileJob->setBandwidthLimited(true);
        getFileJob->setChoked(false);
        getFileJob->giveBandwidthQuota(quotaPerJob);

        qCDebug(lcBandwidthManager) << "Gave" << quotaPerJob / 1024.0 << "kB to" << getFileJob;
    }
    _relativeLimitCurrentMeasuredDevice = nullptr;
}

void BandwidthManager::relativeDownloadDelayTimerExpired()
{
    // Switch to measuring state
    _relativeDownloadMeasuringTimer.start(); // always start to continue the cycle

    if (!usingRelativeDownloadLimit()) {
        return; // oh, not actually needed
    }

    if (_downloadJobList.empty()) {
        qCDebug(lcBandwidthManager) << _downloadJobList.size() << "No jobs?";
        return;
    }

    qCDebug(lcBandwidthManager) << _downloadJobList.size() << "Starting measuring";

    // Take first device and then append it again (= we round robin all devices)
    _relativeLimitCurrentMeasuredJob = _downloadJobList.front();
    _downloadJobList.pop_front();
    _downloadJobList.push_back(_relativeLimitCurrentMeasuredJob);

    _relativeDownloadLimitProgressAtMeasuringRestart = _relativeLimitCurrentMeasuredJob->currentDownloadPosition();
    _relativeLimitCurrentMeasuredJob->setBandwidthLimited(false);
    _relativeLimitCurrentMeasuredJob->setChoked(false);

    // choke all other download jobs
    for (const auto getFileJob : _downloadJobList) {
        if (getFileJob == _relativeLimitCurrentMeasuredJob) {
            continue;
        }

        getFileJob->setBandwidthLimited(true);
        getFileJob->setChoked(true);
    }

    // now we're in measuring state
}

// end downloads

void BandwidthManager::switchingTimerExpired()
{
    const auto newUploadLimit = _propagator->_uploadLimit;
    if (newUploadLimit != _currentUploadLimit) {
        qCInfo(lcBandwidthManager) << "Upload Bandwidth limit changed" << _currentUploadLimit << newUploadLimit;
        _currentUploadLimit = newUploadLimit;

        for (const auto uploadDevice : _relativeUploadDeviceList) {
            Q_ASSERT(uploadDevice);

            if (usingAbsoluteUploadLimit()) {
                uploadDevice->setBandwidthLimited(true);
                uploadDevice->setChoked(false);
            } else if (usingRelativeUploadLimit()) {
                uploadDevice->setBandwidthLimited(true);
                uploadDevice->setChoked(true);
            } else {
                uploadDevice->setBandwidthLimited(false);
                uploadDevice->setChoked(false);
            }
        }
    }

    const auto newDownloadLimit = _propagator->_downloadLimit;
    if (newDownloadLimit != _currentDownloadLimit) {
        qCInfo(lcBandwidthManager) << "Download Bandwidth limit changed" << _currentDownloadLimit << newDownloadLimit;
        _currentDownloadLimit = newDownloadLimit;

        for (const auto getJob : _downloadJobList) {
            Q_ASSERT(getJob);

            if (usingAbsoluteDownloadLimit()) {
                getJob->setBandwidthLimited(true);
                getJob->setChoked(false);
            } else if (usingRelativeDownloadLimit()) {
                getJob->setBandwidthLimited(true);
                getJob->setChoked(true);
            } else {
                getJob->setBandwidthLimited(false);
                getJob->setChoked(false);
            }
        }
    }
}

void BandwidthManager::absoluteLimitTimerExpired()
{
    if (usingAbsoluteUploadLimit() && !_absoluteUploadDeviceList.empty()) {
        const auto quotaPerDevice = _currentUploadLimit / qMax((std::list<UploadDevice *>::size_type)1, _absoluteUploadDeviceList.size());

        qCDebug(lcBandwidthManager) << quotaPerDevice << _absoluteUploadDeviceList.size() << _currentUploadLimit;

        for (const auto device : _absoluteUploadDeviceList) {
            device->giveBandwidthQuota(quotaPerDevice);
            qCDebug(lcBandwidthManager) << "Gave " << quotaPerDevice / 1024.0 << " kB to" << device;
        }
    }

    if (usingAbsoluteDownloadLimit() && !_downloadJobList.empty()) {
        const auto quotaPerJob = _currentDownloadLimit / qMax((std::list<GETFileJob *>::size_type)1, _downloadJobList.size());

        qCDebug(lcBandwidthManager) << quotaPerJob << _downloadJobList.size() << _currentDownloadLimit;

        for (const auto job : _downloadJobList) {
            job->giveBandwidthQuota(quotaPerJob);
            qCDebug(lcBandwidthManager) << "Gave " << quotaPerJob / 1024.0 << " kB to" << job;
        }
    }
}

} // namespace OCC
