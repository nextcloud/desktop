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

#include <QBuffer>

#include "owncloudpropagator_p.h"
#include "networkjobs.h"


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
    explicit PUTFileJob(Account* account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, QObject* parent = 0)
    : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers) {}

    virtual void start();

    virtual void finished() {
        emit finishedSignal();
    }

signals:
    void finishedSignal();
};

class PropagateUploadFileQNAM : public PropagateItemJob {
    Q_OBJECT
public:
    PropagateUploadFileQNAM(OwncloudPropagator* propagator,const SyncFileItem& item)  : PropagateItemJob(propagator, item) {}
    void start();
private slots:
    void slotPutFinished();
};

class ChunkedPUTFileJob : public AbstractNetworkJob {
    Q_OBJECT

private:
    QIODevice* _device;
    QMap<QByteArray, QByteArray> _headers;
    QMap <PUTFileJob*, ChunkBlock>_chunkUploadJobs;
    QVector<ChunkBlock> _chunks;
    time_t _modtime;
    SyncFileItem _item;
    int _transferId;
    int _transferedChunks;

public:
    explicit ChunkedPUTFileJob(Account* account, const QString& path, SyncFileItem& item, QIODevice *device,
                               QObject* parent = 0) :
        AbstractNetworkJob(account, path, parent), _device(device), _item(item)
    {}

    virtual void start();

    virtual void finished() {
        emit finishedSignal();
    }

    int computeChunks();

private slots:
    void slotChunkFinished();

signals:
    void finishedSignal();
};


}
