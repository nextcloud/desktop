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
#include <QElapsedTimer>


namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcPutJob)
Q_DECLARE_LOGGING_CATEGORY(lcPropagateUpload)

class BandwidthManager;

/**
 * @brief The UploadDevice class
 * @ingroup libsync
 */
class UploadDevice : public QIODevice
{
    Q_OBJECT
public:
    UploadDevice(BandwidthManager *bwm);
    ~UploadDevice();

    /** Reads the data from the file and opens the device */
    bool prepareAndOpen(const QString &fileName, qint64 start, qint64 size);

    qint64 writeData(const char *, qint64) Q_DECL_OVERRIDE;
    qint64 readData(char *data, qint64 maxlen) Q_DECL_OVERRIDE;
    bool atEnd() const Q_DECL_OVERRIDE;
    qint64 size() const Q_DECL_OVERRIDE;
    qint64 bytesAvailable() const Q_DECL_OVERRIDE;
    bool isSequential() const Q_DECL_OVERRIDE;
    bool seek(qint64 pos) Q_DECL_OVERRIDE;

    void setBandwidthLimited(bool);
    bool isBandwidthLimited() { return _bandwidthLimited; }
    void setChoked(bool);
    bool isChoked() { return _choked; }
    void giveBandwidthQuota(qint64 bwq);

signals:

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
public slots:
    void slotJobUploadProgress(qint64 sent, qint64 t);
};

/**
 * @brief The PUTFileJob class
 * @ingroup libsync
 */
class PUTFileJob : public AbstractNetworkJob
{
    Q_OBJECT

private:
    QIODevice *_device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;
    QUrl _url;
    QElapsedTimer _requestTimer;

public:
    // Takes ownership of the device
    explicit PUTFileJob(AccountPtr account, const QString &path, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject *parent = 0)
        : AbstractNetworkJob(account, path, parent)
        , _device(device)
        , _headers(headers)
        , _chunk(chunk)
    {
        _device->setParent(this);
    }
    explicit PUTFileJob(AccountPtr account, const QUrl &url, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject *parent = 0)
        : AbstractNetworkJob(account, QString(), parent)
        , _device(device)
        , _headers(headers)
        , _url(url)
        , _chunk(chunk)
    {
        _device->setParent(this);
    }
    ~PUTFileJob();

    int _chunk;

    virtual void start() Q_DECL_OVERRIDE;

    virtual bool finished() Q_DECL_OVERRIDE
    {
        qCInfo(lcPutJob) << "PUT of" << reply()->request().url().toString() << "FINISHED WITH STATUS"
                         << reply()->error()
                         << (reply()->error() == QNetworkReply::NoError ? QLatin1String("") : errorString())
                         << reply()->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                         << reply()->attribute(QNetworkRequest::HttpReasonPhraseAttribute);

        emit finishedSignal();
        return true;
    }

    QIODevice *device()
    {
        return _device;
    }

    QString errorString()
    {
        return _errorString.isEmpty() ? AbstractNetworkJob::errorString() : _errorString;
    }

    quint64 msSinceStart() const
    {
        return _requestTimer.elapsed();
    }

signals:
    void finishedSignal();
    void uploadProgress(qint64, qint64);

};

/**
 * @brief This job implements the asynchronous PUT
 *
 * If the server replies to a PUT with a OC-Finish-Poll url, we will query this url until the server
 * replies with an etag. https://github.com/owncloud/core/issues/12097
 * @ingroup libsync
 */
class PollJob : public AbstractNetworkJob
{
    Q_OBJECT
    SyncJournalDb *_journal;
    QString _localPath;

public:
    SyncFileItemPtr _item;
    // Takes ownership of the device
    explicit PollJob(AccountPtr account, const QString &path, const SyncFileItemPtr &item,
        SyncJournalDb *journal, const QString &localPath, QObject *parent)
        : AbstractNetworkJob(account, path, parent)
        , _journal(journal)
        , _localPath(localPath)
        , _item(item)
    {
    }

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
class PropagateUploadFileCommon : public PropagateItemJob
{
    Q_OBJECT

protected:
    QVector<AbstractNetworkJob *> _jobs; /// network jobs that are currently in transit
    bool _finished BITFIELD(1); /// Tells that all the jobs have been finished
    bool _deleteExisting BITFIELD(1);
    quint64 _abortCount; /// Keep track of number of aborted items

// measure the performance of checksum calc and upload
#ifdef WITH_TESTING
    Utility::StopWatch _stopWatch;
#endif

    QByteArray _transmissionChecksumHeader;

public:
    PropagateUploadFileCommon(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
        , _finished(false)
        , _deleteExisting(false)
        , _abortCount(0)
    {
    }

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
    void slotComputeTransmissionChecksum(const QByteArray &contentChecksumType, const QByteArray &contentChecksum);
    // transmission checksum computed, prepare the upload
    void slotStartUpload(const QByteArray &transmissionChecksumType, const QByteArray &transmissionChecksum);

public:
    virtual void doStartUpload() = 0;

    void startPollJob(const QString &path);
    void finalize();
    void abortWithError(SyncFileItem::Status status, const QString &error);

public slots:
    void abort(PropagatorJob::AbortType abortType) Q_DECL_OVERRIDE;
    void slotJobDestroyed(QObject *job);

private slots:
    void slotReplyAbortFinished();
    void slotPollFinished();

protected:
    /**
     * Prepares the abort e.g. connects proper signals and slots
     * to the subjobs to abort asynchronously
     */
    void prepareAbort(PropagatorJob::AbortType abortType);

    /**
     * Checks whether the current error is one that should reset the whole
     * transfer if it happens too often. If so: Bump UploadInfo::errorCount
     * and maybe perform the reset.
     */
    void checkResettingErrors();

    /**
     * Error handling functionality that is shared between jobs.
     */
    void commonErrorHandling(AbstractNetworkJob *job);

    /**
     * Increases the timeout for the final MOVE/PUT for large files.
     *
     * This is an unfortunate workaround since the drawback is not being able to
     * detect real disconnects in a timely manner. Shall go away when the server
     * response starts coming quicker, or there is some sort of async api.
     *
     * See #6527, enterprise#2480
     */
    static void adjustLastJobTimeout(AbstractNetworkJob *job, quint64 fileSize);

    // Bases headers that need to be sent with every chunk
    QMap<QByteArray, QByteArray> headers();
};

/**
 * @ingroup libsync
 *
 * Propagation job, impementing the old chunking agorithm
 *
 */
class PropagateUploadFileV1 : public PropagateUploadFileCommon
{
    Q_OBJECT

private:
    /**
     * That's the start chunk that was stored in the database for resuming.
     * In the non-resuming case it is 0.
     * If we are resuming, this is the first chunk we need to send
     */
    int _startChunk = 0;
    /**
     * This is the next chunk that we need to send. Starting from 0 even if _startChunk != 0
     * (In other words,  _startChunk + _currentChunk is really the number of the chunk we need to send next)
     * (In other words, _currentChunk is the number of the chunk that we already sent or started sending)
     */
    int _currentChunk = 0;
    int _chunkCount = 0; /// Total number of chunks for this file
    int _transferId = 0; /// transfer id (part of the url)

    quint64 chunkSize() const {
        // Old chunking does not use dynamic chunking algorithm, and does not adjusts the chunk size respectively,
        // thus this value should be used as the one classifing item to be chunked
        return propagator()->syncOptions()._initialChunkSize;
    }

public:
    PropagateUploadFileV1(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateUploadFileCommon(propagator, item)
    {
    }

    void doStartUpload() Q_DECL_OVERRIDE;
public slots:
    void abort(PropagatorJob::AbortType abortType) Q_DECL_OVERRIDE;
private slots:
    void startNextChunk();
    void slotPutFinished();
    void slotUploadProgress(qint64, qint64);
};

/**
 * @ingroup libsync
 *
 * Propagation job, impementing the new chunking agorithm
 *
 */
class PropagateUploadFileNG : public PropagateUploadFileCommon
{
    Q_OBJECT
private:
    quint64 _sent = 0; /// amount of data (bytes) that was already sent
    uint _transferId = 0; /// transfer id (part of the url)
    int _currentChunk = 0; /// Id of the next chunk that will be sent
    quint64 _currentChunkSize = 0; /// current chunk size
    bool _removeJobError = false; /// If not null, there was an error removing the job

    // Map chunk number with its size  from the PROPFIND on resume.
    // (Only used from slotPropfindIterate/slotPropfindFinished because the LsColJob use signals to report data.)
    struct ServerChunkInfo
    {
        quint64 size;
        QString originalName;
    };
    QMap<int, ServerChunkInfo> _serverChunks;

    /**
     * Return the URL of a chunk.
     * If chunk == -1, returns the URL of the parent folder containing the chunks
     */
    QUrl chunkUrl(int chunk = -1);

public:
    PropagateUploadFileNG(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateUploadFileCommon(propagator, item)
    {
    }

    void doStartUpload() Q_DECL_OVERRIDE;

private:
    void startNewUpload();
    void startNextChunk();
public slots:
    void abort(AbortType abortType) Q_DECL_OVERRIDE;
private slots:
    void slotPropfindFinished();
    void slotPropfindFinishedWithError();
    void slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties);
    void slotDeleteJobFinished();
    void slotMkColFinished(QNetworkReply::NetworkError);
    void slotPutFinished();
    void slotMoveJobFinished();
    void slotUploadProgress(qint64, qint64);
};
}
