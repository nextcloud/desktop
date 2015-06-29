/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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

#include "owncloudpropagator.h"
#include "networkjobs.h"

#include <QBuffer>
#include <QFile>

namespace OCC {

/**
 * @brief The GETFileJob class
 * @ingroup libsync
 */
class GETFileJob : public AbstractNetworkJob {
    Q_OBJECT
    QFile* _device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;
    QByteArray _expectedEtagForResume;
    quint64 _resumeStart;
    SyncFileItem::Status _errorStatus;
    QUrl _directDownloadUrl;
    QByteArray _etag;
    bool _bandwidthLimited; // if _bandwidthQuota will be used
    bool _bandwidthChoked; // if download is paused (won't read on readyRead())
    qint64 _bandwidthQuota;
    QPointer<BandwidthManager> _bandwidthManager;
    bool _hasEmittedFinishedSignal;
    time_t _lastModified;
public:

    // DOES NOT take owncership of the device.
    explicit GETFileJob(AccountPtr account, const QString& path, QFile *device,
                        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
                        quint64 resumeStart, QObject* parent = 0);
    // For directDownloadUrl:
    explicit GETFileJob(AccountPtr account, const QUrl& url, QFile *device,
                        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
                        quint64 resumeStart, QObject* parent = 0);
    virtual ~GETFileJob() {
        if (_bandwidthManager) {
            _bandwidthManager->unregisterDownloadJob(this);
        }
    }

    virtual void start() Q_DECL_OVERRIDE;
    virtual bool finished() Q_DECL_OVERRIDE {
//         qDebug() << Q_FUNC_INFO << reply()->bytesAvailable() << _hasEmittedFinishedSignal;
        if (reply()->bytesAvailable()) {
//             qDebug() << Q_FUNC_INFO << "Not all read yet because of bandwidth limits";
            return false;
        } else {
            if (_bandwidthManager) {
                _bandwidthManager->unregisterDownloadJob(this);
            }
            if (!_hasEmittedFinishedSignal) {
                emit finishedSignal();
            }
            _hasEmittedFinishedSignal = true;
            return true; // discard
        }
    }

    void setBandwidthManager(BandwidthManager *bwm);
    void setChoked(bool c);
    void setBandwidthLimited(bool b);
    void giveBandwidthQuota(qint64 q);
    qint64 currentDownloadPosition();

    QString errorString() const;
    void setErrorString(const QString& s) { _errorString = s; }

    SyncFileItem::Status errorStatus() { return _errorStatus; }
    void setErrorStatus(const SyncFileItem::Status & s) { _errorStatus = s; }

    virtual void slotTimeout() Q_DECL_OVERRIDE;

    QByteArray &etag() { return _etag; }
    quint64 resumeStart() { return _resumeStart; }
    time_t lastModified() { return _lastModified; }


signals:
    void finishedSignal();
    void downloadProgress(qint64,qint64);
private slots:
    void slotReadyRead();
    void slotMetaDataChanged();
};

/**
 * @brief The PropagateDownloadFileQNAM class
 * @ingroup libsync
 */
class PropagateDownloadFileQNAM : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateDownloadFileQNAM(OwncloudPropagator* propagator,const SyncFileItemPtr& item)
        : PropagateItemJob(propagator, item) {}
    void start() Q_DECL_OVERRIDE;

private slots:
    void slotGetFinished();
    void abort() Q_DECL_OVERRIDE;
    void downloadFinished();
    void slotDownloadProgress(qint64,qint64);
    void slotChecksumFail( const QString& errMsg );

private:
    QPointer<GETFileJob> _job;
    QFile _tmpFile;
};

}
