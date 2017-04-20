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
#include <QElapsedTimer>


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
    QIODevice* _device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;
    QUrl _url;
    QElapsedTimer _requestTimer;

public:
    // Takes ownership of the device
    explicit PUTFileJob(AccountPtr account, const QString& path, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject* parent = 0)
        : AbstractNetworkJob(account, path, parent), _device(device), _headers(headers), _chunk(chunk)
    {
        _device->setParent(this);
    }
    explicit PUTFileJob(AccountPtr account, const QUrl& url, QIODevice *device,
                        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject* parent = 0)
        : AbstractNetworkJob(account, QString(), parent), _device(device), _headers(headers)
        , _url(url), _chunk(chunk)
    {
        _device->setParent(this);
    }
    ~PUTFileJob();

    int _chunk;

    virtual void start() Q_DECL_OVERRIDE;

    virtual bool finished() Q_DECL_OVERRIDE {
        emit finishedSignal();
        return true;
    }

    QString errorString() {
        return _errorString.isEmpty() ? AbstractNetworkJob::errorString() : _errorString;
    }

    quint64 msSinceStart() const {
        return _requestTimer.elapsed();
    }

signals:
    void finishedSignal();
    void uploadProgress(qint64,qint64);

private slots:
#if QT_VERSION < 0x050402
    void slotSoftAbort();
#endif
};

/**
 * @brief This job implements the asynchronous PUT
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

signals:
    void finishedSignal();
};

/**
 * @brief The PropagateUploadFileCommon class is the code common between all chunking algorithms
 * @ingroup libsync
 *
 * State Machine:
 *
 *   +---> start()  --> (delete job) -------+
 *   |                                      |
 *   +--> slotComputeContentChecksum()  <---+
 *                   |
 *                   v
 *    slotComputeTransmissionChecksum()
 *         |
 *         v
 *    slotStartUpload()  -> doStartUpload()
 *                                  .
 *                                  .
 *                                  v
 *        finalize() or abortWithError()  or startPollJob()
 */
class PropagateUploadFileCommon : public PropagateItemJob {
    Q_OBJECT

protected:
    QVector<AbstractNetworkJob*> _jobs; /// network jobs that are currently in transit
    bool _finished BITFIELD(1); /// Tells that all the jobs have been finished
    bool _deleteExisting BITFIELD(1);

    // measure the performance of checksum calc and upload
#ifdef WITH_TESTING
    Utility::StopWatch _stopWatch;
#endif

    QByteArray _transmissionChecksum;
    QByteArray _transmissionChecksumType;

public:
    PropagateUploadFileCommon(OwncloudPropagator* propagator,const SyncFileItemPtr& item)
        : PropagateItemJob(propagator, item), _finished(false), _deleteExisting(false) {}

    /**
     * Whether an existing entity with the same name may be deleted before
     * the upload.
     *
     * Default: false.
     */
    void setDeleteExisting(bool enabled);

    void start() Q_DECL_OVERRIDE;

    bool isLikelyFinishedQuickly() Q_DECL_OVERRIDE { return _item->_size < propagator()->smallFileSize(); }

private slots:
    void slotComputeContentChecksum();
    // Content checksum computed, compute the transmission checksum
    void slotComputeTransmissionChecksum(const QByteArray& contentChecksumType, const QByteArray& contentChecksum);
    // transmission checksum computed, prepare the upload
    void slotStartUpload(const QByteArray& transmissionChecksumType, const QByteArray& transmissionChecksum);
public:
    virtual void doStartUpload() = 0;

    void startPollJob(const QString& path);
    void finalize();
    void abortWithError(SyncFileItem::Status status, const QString &error);

public slots:
    void abort() Q_DECL_OVERRIDE;
    void slotJobDestroyed(QObject *job);

private slots:
    void slotPollFinished();

protected:
    /**
     * Checks whether the current error is one that should reset the whole
     * transfer if it happens too often. If so: Bump UploadInfo::errorCount
     * and maybe perform the reset.
     */
    void checkResettingErrors();

    // Bases headers that need to be sent with every chunk
    QMap<QByteArray, QByteArray> headers();

};

/**
 * @ingroup libsync
 *
 * Propagation job, impementing the old chunking agorithm
 *
 */
class PropagateUploadFileV1 : public PropagateUploadFileCommon {
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
     * (In other words, _currentChunk is the number of the chunk that we already sent or started sending)
     */
    int _currentChunk;
    int _chunkCount; /// Total number of chunks for this file
    int _transferId; /// transfer id (part of the url)

    quint64 chunkSize() const { return propagator()->syncOptions()._initialChunkSize; }


public:
    PropagateUploadFileV1(OwncloudPropagator* propagator,const SyncFileItemPtr& item) :
        PropagateUploadFileCommon(propagator,item) {}

    void doStartUpload() Q_DECL_OVERRIDE;

private slots:
    void startNextChunk();
    void slotPutFinished();
    void slotUploadProgress(qint64,qint64);
};

/**
 * @ingroup libsync
 *
 * Propagation job, impementing the new chunking agorithm
 *
 */
class PropagateUploadFileNG : public PropagateUploadFileCommon {
    Q_OBJECT
private:
    quint64 _sent; /// amount of data (bytes) that was already sent
    uint _transferId; /// transfer id (part of the url)
    int _currentChunk; /// Id of the next chunk that will be sent
    quint64 _currentChunkSize; /// current chunk size
    bool _removeJobError; /// If not null, there was an error removing the job

    // Map chunk number with its size  from the PROPFIND on resume.
    // (Only used from slotPropfindIterate/slotPropfindFinished because the LsColJob use signals to report data.)
    struct ServerChunkInfo { quint64 size; QString originalName; };
    QMap<int, ServerChunkInfo> _serverChunks;

    /**
     * Return the URL of a chunk.
     * If chunk == -1, returns the URL of the parent folder containing the chunks
     */
    QUrl chunkUrl(int chunk = -1);

public:
    PropagateUploadFileNG(OwncloudPropagator* propagator,const SyncFileItemPtr& item) :
        PropagateUploadFileCommon(propagator,item), _currentChunkSize(0) {}

    void doStartUpload() Q_DECL_OVERRIDE;

private:
    void startNewUpload();
    void startNextChunk();
private slots:
    void slotPropfindFinished();
    void slotPropfindFinishedWithError();
    void slotPropfindIterate(const QString &name, const QMap<QString,QString> &properties);
    void slotDeleteJobFinished();
    void slotMkColFinished(QNetworkReply::NetworkError);
    void slotPutFinished();
    void slotMoveJobFinished();
    void slotUploadProgress(qint64,qint64);
};


}

