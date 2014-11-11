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
#include <QDebug>

namespace OCC {

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

    virtual void start() Q_DECL_OVERRIDE;

    virtual bool finished() Q_DECL_OVERRIDE {
        emit finishedSignal();
        return true;
    }

    QString errorString() {
        return _errorString.isEmpty() ? reply()->errorString() : _errorString;
    };

    virtual void slotTimeout() Q_DECL_OVERRIDE;


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
    void start() Q_DECL_OVERRIDE;
private slots:
    void slotPutFinished();
    void slotPollFinished();
    void slotUploadProgress(qint64,qint64);
    void abort() Q_DECL_OVERRIDE;
    void startNextChunk();
    void finalize(const SyncFileItem&);
    void slotJobDestroyed(QObject *job);
private:
    void startPollJob(const QString& path);
};

}

