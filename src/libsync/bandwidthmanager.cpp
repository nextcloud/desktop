/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

// FIXME At some point:
//  * Register device only after the QNR received its metaDataChanged() signal
//  * Incorporate Qt buffer fill state (it's a negative absolute delta).
//  * Incorporate SSL overhead (percentage)

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

    // absolute uploads/downloads — only start the timer when a limit is actually configured
    QObject::connect(&_absoluteLimitTimer, &QTimer::timeout, this, &BandwidthManager::absoluteLimitTimerExpired);
    _absoluteLimitTimer.setInterval(1000);
    if (usingAbsoluteUploadLimit() || usingAbsoluteDownloadLimit()) {
        _absoluteLimitTimer.start();
    }

}

BandwidthManager::~BandwidthManager() = default;

void BandwidthManager::registerUploadDevice(UploadDevice *p)
{
    _absoluteUploadDeviceList.push_back(p);
    _dirty = true;
    QObject::connect(p, &QObject::destroyed, this, &BandwidthManager::unregisterUploadDevice);

    if (usingAbsoluteUploadLimit()) {
        p->setBandwidthLimited(true);
        p->setChoked(false);
    } else {
        p->setBandwidthLimited(false);
        p->setChoked(false);
    }
}

void BandwidthManager::unregisterUploadDevice(QObject *o)
{
    auto p = reinterpret_cast<UploadDevice *>(o); // note, we might already be in the ~QObject
    _absoluteUploadDeviceList.remove(p);
    _dirty = true;
}

void BandwidthManager::registerDownloadJob(GETFileJob *j)
{
    _downloadJobList.push_back(j);
    _dirty = true;
    QObject::connect(j, &QObject::destroyed, this, &BandwidthManager::unregisterDownloadJob);

    if (usingAbsoluteDownloadLimit()) {
        j->setBandwidthLimited(true);
        j->setChoked(false);
    } else {
        j->setBandwidthLimited(false);
        j->setChoked(false);
    }
}

void BandwidthManager::unregisterDownloadJob(QObject *o)
{
    auto *j = reinterpret_cast<GETFileJob *>(o); // note, we might already be in the ~QObject
    _downloadJobList.remove(j);
    _dirty = true;
}

void BandwidthManager::switchingTimerExpired()
{
    if (!_dirty) {
        return;
    }
    _dirty = false;

    const auto newUploadLimit = _propagator->_uploadLimit;
    if (newUploadLimit != _currentUploadLimit) {
        qCInfo(lcBandwidthManager) << "Upload Bandwidth limit changed" << _currentUploadLimit << newUploadLimit;
        _currentUploadLimit = newUploadLimit;

        for (const auto uploadDevice : _absoluteUploadDeviceList) {
            Q_ASSERT(uploadDevice);

            if (usingAbsoluteUploadLimit()) {
                uploadDevice->setBandwidthLimited(true);
                uploadDevice->setChoked(false);
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
            } else {
                getJob->setBandwidthLimited(false);
                getJob->setChoked(false);
            }
        }
    }

    // Start or stop the absolute-limit timer based on whether any limit is now active.
    if (usingAbsoluteUploadLimit() || usingAbsoluteDownloadLimit()) {
        if (!_absoluteLimitTimer.isActive()) {
            _absoluteLimitTimer.start();
        }
    } else {
        _absoluteLimitTimer.stop();
    }
}

void BandwidthManager::absoluteLimitTimerExpired()
{
    if (!_dirty) {
        return;
    }
    _dirty = false;

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
