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
    QIODevice* _device;
    QMap<QByteArray, QByteArray> _headers;

public:
    // Takes ownership of the device
    explicit PUTFileJob(Account* account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, QObject* parent = 0)
    : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers) {}

    virtual void start();

    virtual void finished() {
        emit finishedSignal();
    }

signals:
    void finishedSignal();
    void uploadProgress(qint64,qint64);
};


class PropagateUploadFileQNAM : public PropagateItemJob {
    Q_OBJECT
    QPointer<PUTFileJob> _job;
    QFile *_file;
    int _startChunk;
    int _currentChunk;
    int _chunkCount;
    int _transferId;
public:
    PropagateUploadFileQNAM(OwncloudPropagator* propagator,const SyncFileItem& item)
        : PropagateItemJob(propagator, item), _startChunk(0), _currentChunk(0), _chunkCount(0), _transferId(0) {}
    void start();
private slots:
    void slotPutFinished();
    void slotUploadProgress(qint64,qint64);
    void abort();
    void startNextChunk();

};


class GETFileJob : public AbstractNetworkJob {
    Q_OBJECT
    QIODevice* _device;
    QMap<QByteArray, QByteArray> _headers;
public:
    // DOES NOT take owncership of the device.
    explicit GETFileJob(Account* account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, QObject* parent = 0)
    : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers) {}

    virtual void start();
    virtual void finished() {
        emit finishedSignal();
    }

signals:
    void finishedSignal();
    void downloadProgress(qint64,qint64);
private slots:
    void slotReadyRead();
};


class PropagateDownloadFileQNAM : public PropagateItemJob {
    Q_OBJECT
    QPointer<GETFileJob> _job;
    QByteArray _expectedEtagForResume;

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
