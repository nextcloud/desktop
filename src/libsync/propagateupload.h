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
class BandwidthManager;

class UploadDevice : public QIODevice {
    Q_OBJECT
public:
    QPointer<QIODevice> _file;
    qint64 _read;
    qint64 _size;
    qint64 _start;
    BandwidthManager* _bandwidthManager;

    qint64 _bandwidthQuota;
    qint64 _readWithProgress;

    UploadDevice(QIODevice *file,  qint64 start, qint64 size, BandwidthManager *bwm);
    ~UploadDevice();
    virtual qint64 writeData(const char* , qint64 );
    virtual qint64 readData(char* data, qint64 maxlen);
    virtual bool atEnd() const;
    virtual qint64 size() const;
    qint64 bytesAvailable() const;
    virtual bool isSequential() const;
    virtual bool seek ( qint64 pos );

    void setBandwidthLimited(bool);
    bool isBandwidthLimited() { return _bandwidthLimited; }
    void setChoked(bool);
    bool isChoked() { return _choked; }
    void giveBandwidthQuota(qint64 bwq);
private:
    bool _bandwidthLimited; // if _bandwidthQuota will be used
    bool _choked; // if upload is paused (readData() will return 0)
protected slots:
    void slotJobUploadProgress(qint64 sent, qint64 t);
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

