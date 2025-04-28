/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BANDWIDTHMANAGER_H
#define BANDWIDTHMANAGER_H

#include <QObject>
#include <QTimer>
#include <QIODevice>
#include <list>

namespace OCC {

class UploadDevice;
class GETFileJob;
class OwncloudPropagator;

/**
 * @brief The BandwidthManager class
 * @ingroup libsync
 */
class BandwidthManager : public QObject
{
    Q_OBJECT
public:
    BandwidthManager(OwncloudPropagator *p);
    ~BandwidthManager() override;

    bool usingAbsoluteUploadLimit() { return _currentUploadLimit > 0; }
    bool usingRelativeUploadLimit() { return _currentUploadLimit < 0; }
    bool usingAbsoluteDownloadLimit() { return _currentDownloadLimit > 0; }
    bool usingRelativeDownloadLimit() { return _currentDownloadLimit < 0; }


public slots:
    void registerUploadDevice(OCC::UploadDevice *);
    void unregisterUploadDevice(QObject *);

    void registerDownloadJob(OCC::GETFileJob *);
    void unregisterDownloadJob(QObject *);

    void absoluteLimitTimerExpired();
    void switchingTimerExpired();

    void relativeUploadMeasuringTimerExpired();
    void relativeUploadDelayTimerExpired();

    void relativeDownloadMeasuringTimerExpired();
    void relativeDownloadDelayTimerExpired();

private:
    // for switching between absolute and relative bw limiting
    QTimer _switchingTimer;

    // FIXME this timer and this variable should be replaced
    // by the propagator emitting the changed limit values to us as signal
    OwncloudPropagator *_propagator;

    // for absolute up/down bw limiting
    QTimer _absoluteLimitTimer;

    // FIXME merge these two lists
    std::list<UploadDevice *> _absoluteUploadDeviceList;
    std::list<UploadDevice *> _relativeUploadDeviceList;

    QTimer _relativeUploadMeasuringTimer;

    // for relative bw limiting, we need to wait this amount before measuring again
    QTimer _relativeUploadDelayTimer;

    // the device measured
    UploadDevice *_relativeLimitCurrentMeasuredDevice = nullptr;

    // for measuring how much progress we made at start
    qint64 _relativeUploadLimitProgressAtMeasuringRestart = 0;
    qint64 _currentUploadLimit = 0;

    std::list<GETFileJob *> _downloadJobList;
    QTimer _relativeDownloadMeasuringTimer;

    // for relative bw limiting, we need to wait this amount before measuring again
    QTimer _relativeDownloadDelayTimer;

    // the device measured
    GETFileJob *_relativeLimitCurrentMeasuredJob = nullptr;

    // for measuring how much progress we made at start
    qint64 _relativeDownloadLimitProgressAtMeasuringRestart = 0LL;

    qint64 _currentDownloadLimit = 0;
};

} // namespace OCC

#endif
