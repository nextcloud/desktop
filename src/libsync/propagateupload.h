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

/**
 * @brief The UploadDevice class
 * @ingroup libsync
 */
class UploadDevice : public QIODevice {
    Q_OBJECT
public:
    UploadDevice(BandwidthManager *bwm);
    ~UploadDevice();

    /** Reads the data from the file and opens the device */
    bool prepareAndOpen(const QString& fileName, qint64 start, qint64 size);

    qint64 writeData(const char* , qint64 ) Q_DECL_OVERRIDE;
    qint64 readData(char* data, qint64 maxlen) Q_DECL_OVERRIDE;
    bool atEnd() const Q_DECL_OVERRIDE;
    qint64 size() const Q_DECL_OVERRIDE;
    qint64 bytesAvailable() const Q_DECL_OVERRIDE;
    bool isSequential() const Q_DECL_OVERRIDE;
    bool seek ( qint64 pos ) Q_DECL_OVERRIDE;

#if QT_VERSION < QT_VERSION_CHECK(5, 4, 2)
    bool reset() Q_DECL_OVERRIDE { emit wasReset(); return QIODevice::reset(); }
#endif

    void setBandwidthLimited(bool);
    bool isBandwidthLimited() { return _bandwidthLimited; }
    void setChoked(bool);
    bool isChoked() { return _choked; }
    void giveBandwidthQuota(qint64 bwq);

signals:
#if QT_VERSION < 0x050402
    void wasReset();
#endif

private:

    // The file data
    QByteArray _data;
    // Position in the data
    qint64 _read;

    // Bandwidth manager related
    QPointer<BandwidthManager> _bandwidthManager;
    qint64 _bandwidthQuota;
    qint64 _readWithProgress;
    bool _bandwidthLimited; // if _bandwidthQuota will be used
    bool _choked; // if upload is paused (readData() will return 0)
    friend class BandwidthManager;
protected slots:
    void slotJobUploadProgress(qint64 sent, qint64 t);
};

/**
 * @brief The PUTFileJob class
 * @ingroup libsync
 */
class PUTFileJob : public AbstractNetworkJob {
    Q_OBJECT

private:
    QScopedPointer<QIODevice> _device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;

public:
    // Takes ownership of the device
    explicit PUTFileJob(AccountPtr account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject* parent = 0)
        : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers), _chunk(chunk) {}
    ~PUTFileJob();

    int _chunk;

    virtual void start() Q_DECL_OVERRIDE;

    virtual bool finished() Q_DECL_OVERRIDE {
        emit finishedSignal();
        return true;
    }

    QString errorString() {
        return _errorString.isEmpty() ? reply()->errorString() : _errorString;
    }

    virtual void slotTimeout() Q_DECL_OVERRIDE;


signals:
    void finishedSignal();
    void uploadProgress(qint64,qint64);

private slots:
#if QT_VERSION < 0x050402
    void slotSoftAbort();
#endif
};

/**
 * @brief This job implements the assynchronous PUT
 *
 * If the server replies to a PUT with a OC-Finish-Poll url, we will query this url until the server
 * replies with an etag. https://github.com/owncloud/core/issues/12097
 * @ingroup libsync
 */
class PollJob : public AbstractNetworkJob {
    Q_OBJECT
    SyncJournalDb *_journal;
    QString _localPath;
public:
    SyncFileItemPtr _item;
    // Takes ownership of the device
    explicit PollJob(AccountPtr account, const QString &path, const SyncFileItemPtr &item,
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

/**
 * @brief The PropagateUploadFileQNAM class
 * @ingroup libsync
 */
class PropagateUploadFileQNAM : public PropagateItemJob {
    Q_OBJECT

private:
    /**
     * That's the start chunk that was stored in the database for resuming.
     * In the non-resuming case it is 0.
     * If we are resuming, this is the first chunk we need to send
     */
    int _startChunk;
    /**
     * This is the next chunk that we need to send. Starting from 0 even if _startChunk != 0
     * (In other words,  _startChunk + _currentChunk is really the number of the chunk we need to send next)
     * (In other words, _currentChunk is the number of chunk that we already sent or start sending)
     */
    int _currentChunk;
    int _chunkCount; /// Total number of chunks for this file
    int _transferId; /// transfer id (part of the url)
    QElapsedTimer _duration;
    QVector<PUTFileJob*> _jobs; /// network jobs that are currently in transit
    bool _finished; // Tells that all the jobs have been finished

    // measure the performance of checksum calc and upload
    Utility::StopWatch _stopWatch;

public:
    PropagateUploadFileQNAM(OwncloudPropagator* propagator,const SyncFileItemPtr& item)
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
    void slotStartUpload(const QByteArray &checksum);

private:
    void startPollJob(const QString& path);
    void abortWithError(SyncFileItem::Status status, const QString &error);
};

}

