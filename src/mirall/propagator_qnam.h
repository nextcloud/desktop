/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
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
#pragma once


#include "owncloudpropagator.h"
#include "owncloudpropagator_p.h"
#include "networkjobs.h"

#include <QBuffer>
#include <QFile>

namespace Mirall {

class ChunkBlock {

public:
    explicit ChunkBlock() : _state(NotTransfered) { }
    enum State {
        CHUNK_SUCCESS,
        NotTransfered,           /* never tried to transfer     */
        Transfered,              /* transfer currently running  */
        TransferFailed,          /* transfer tried but failed   */
        TransferSuccess,         /* block transfer succeeded.   */
        Fail
    };

    int      _sequenceNo;
    int64_t  _start;
    int64_t  _size;

    State    _state;
    int      _httpResultCode;
    QString  _httpErrorMsg;
    QString  _etag;
    QBuffer *_buffer;

};

class PUTFileJob : public AbstractNetworkJob {
    Q_OBJECT
    QSharedPointer<QIODevice> _device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;

public:
    // Takes ownership of the device
    explicit PUTFileJob(Account* account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject* parent = 0)
        : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers), _chunk(chunk) {}

    int _chunk;

    virtual void start();

    virtual bool finished() {
        emit finishedSignal();
        return true;
    }

    QString errorString() {
        return _errorString.isEmpty() ? reply()->errorString() : _errorString;
    };

    virtual void slotTimeout();


signals:
    void finishedSignal();
    void uploadProgress(qint64,qint64);
};

class PollJob : public AbstractNetworkJob {
    Q_OBJECT
    SyncJournalDb *_journal;
    QString _localPath;
public:
    SyncFileItem _item;
    // Takes ownership of the device
    explicit PollJob(Account* account, const QString &path, const SyncFileItem &item,
                     SyncJournalDb *journal, const QString &localPath, QObject *parent)
        : AbstractNetworkJob(account, path, parent), _journal(journal), _localPath(localPath), _item(item) {}

    void start() Q_DECL_OVERRIDE;
    bool finished() Q_DECL_OVERRIDE;
    void slotTimeout() Q_DECL_OVERRIDE {
//      emit finishedSignal(false);
//      deleteLater();
        qDebug() << Q_FUNC_INFO;
        reply()->abort();
    }

signals:
    void finishedSignal();
};


class PropagateUploadFileQNAM : public PropagateItemJob {
    Q_OBJECT
    QFile *_file;
    int _startChunk;
    int _currentChunk;
    int _chunkCount;
    int _transferId;
    QElapsedTimer _duration;
    QVector<PUTFileJob*> _jobs;
    bool _finished;
public:
    PropagateUploadFileQNAM(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item), _startChunk(0), _currentChunk(0), _chunkCount(0), _transferId(0), _finished(false) {}
    void start();
private slots:
    void slotPutFinished();
    void slotPollFinished();
    void slotUploadProgress(qint64,qint64);
    void abort();
    void startNextChunk();
    void finalize(const SyncFileItem&);
    void slotJobDestroyed(QObject *job);
private:
    void startPollJob(const QString& path);
};


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
    BandwidthManager *_bandwidthManager;
    bool _hasEmittedFinishedSignal;
public:

    // DOES NOT take owncership of the device.
    explicit GETFileJob(Account* account, const QString& path, QFile *device,
                        const QMap<QByteArray, QByteArray> &headers, QByteArray expectedEtagForResume,
                        quint64 resumeStart, QObject* parent = 0);
    // For directDownloadUrl:
    explicit GETFileJob(Account* account, const QUrl& url, QFile *device,
                        const QMap<QByteArray, QByteArray> &headers,
                        QObject* parent = 0);
    virtual ~GETFileJob() {
        if (_bandwidthManager) {
            _bandwidthManager->unregisterDownloadJob(this);
        }
    }

    virtual void start();
    virtual bool finished() {
        qDebug() << Q_FUNC_INFO << reply()->bytesAvailable() << _hasEmittedFinishedSignal;
        if (reply()->bytesAvailable()) {
            qDebug() << Q_FUNC_INFO << "Not all read yet because of bandwidth limits";
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

    QString errorString() {
        return _errorString.isEmpty() ? reply()->errorString() : _errorString;
    };

    SyncFileItem::Status errorStatus() { return _errorStatus; }

    virtual void slotTimeout();

    QByteArray &etag() { return _etag; }


signals:
    void finishedSignal();
    void downloadProgress(qint64,qint64);
private slots:
    void slotReadyRead();
    void slotMetaDataChanged();
};


class PropagateDownloadFileQNAM : public PropagateItemJob {
    Q_OBJECT
    QPointer<GETFileJob> _job;

//  QFile *_file;
    QFile _tmpFile;
    quint64 _startSize;
public:
    PropagateDownloadFileQNAM(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item), _startSize(0) {}
    void start();
private slots:
    void slotGetFinished();
    void abort();
    void downloadFinished();
    void slotDownloadProgress(qint64,qint64);
};

}
