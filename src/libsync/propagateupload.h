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

#include <unordered_set>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcPutJob)
Q_DECLARE_LOGGING_CATEGORY(lcPropagateUpload)
Q_DECLARE_LOGGING_CATEGORY(lcPropagateUploadV1)
Q_DECLARE_LOGGING_CATEGORY(lcPropagateUploadNG)

class BandwidthManager;

/**
 * @brief The UploadDevice class
 * @ingroup libsync
 */
class UploadDevice : public QIODevice
{
    Q_OBJECT
public:
    UploadDevice(const QString &fileName, qint64 start, qint64 size, BandwidthManager *bwm);
    ~UploadDevice() override;

    bool open(QIODevice::OpenMode mode) override;
    void close() override;

    qint64 writeData(const char *, qint64) override;
    qint64 readData(char *data, qint64 maxlen) override;
    bool atEnd() const override;
    qint64 size() const override;
    qint64 bytesAvailable() const override;
    bool isSequential() const override;
    bool seek(qint64 pos) override;

    void setBandwidthLimited(bool);
    bool isBandwidthLimited() { return _bandwidthLimited; }
    void setChoked(bool);
    bool isChoked() { return _choked; }
    void giveBandwidthQuota(qint64 bwq);

signals:

private:
    /// The local file to read data from
    QFile _file;

    /// Start of the file data to use
    qint64 _start = 0;
    /// Amount of file data after _start to use
    qint64 _size = 0;
    /// Position between _start and _start+_size
    qint64 _read = 0;

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
    QElapsedTimer _requestTimer;

public:
    explicit PUTFileJob(AccountPtr account, const QUrl &url, const QString &path, std::unique_ptr<QIODevice> device,
        const HeaderMap &headers, int chunk, QObject *parent = nullptr);
    ~PUTFileJob() override;

    int _chunk;

    void start() override;

    bool finished() override;

    QIODevice *device()
    {
        return _device;
    }

    QString errorString()
    {
        return _errorString.isEmpty() ? AbstractNetworkJob::errorString() : _errorString;
    }

    std::chrono::milliseconds msSinceStart() const
    {
        return std::chrono::milliseconds(_requestTimer.elapsed());
    }

protected:
    void newReplyHook(QNetworkReply *reply) override;

signals:
    void finishedSignal();
    void uploadProgress(qint64, qint64);

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
 *        finalize() or abortWithError()
 */
class PropagateUploadFileCommon : public PropagateItemJob
{
    Q_OBJECT

protected:
    static const QString fileChangedMessage();

    QVector<AbstractNetworkJob *> _jobs; /// network jobs that are currently in transit
    bool _finished; /// Tells that all the jobs have been finished
    bool _deleteExisting;

    /** Whether an abort is currently ongoing.
     *
     * Important to avoid duplicate aborts since each finishing PUTFileJob might
     * trigger an abort on error.
     */
    bool _aborting;

    QByteArray _transmissionChecksumHeader;

public:
    PropagateUploadFileCommon(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
        , _finished(false)
        , _deleteExisting(false)
        , _aborting(false)
    {
    }

    /**
     * Whether an existing entity with the same name may be deleted before
     * the upload.
     *
     * Default: false.
     */
    void setDeleteExisting(bool enabled);

    void start() override;

    bool isLikelyFinishedQuickly() override { return _item->_size < propagator()->smallFileSize(); }

private slots:
    void slotComputeContentChecksum();
    // Content checksum computed, compute the transmission checksum
    void slotComputeTransmissionChecksum(const QByteArray &contentChecksumType, const QByteArray &contentChecksum);
    // transmission checksum computed, prepare the upload
    void slotStartUpload(const QByteArray &transmissionChecksumType, const QByteArray &transmissionChecksum);

public:
    virtual void doStartUpload() = 0;

    void finalize();
    void abortWithError(SyncFileItem::Status status, const QString &error);

    /***
     * Add job to the list of children
     * The job is automatically removed from the children once its done.
     * It is important that this function is called before any connects.
     */
    void addChildJob(AbstractNetworkJob *job);

    const auto &childJobs() const
    {
        return _childJobs;
    }


protected:
    void done(SyncFileItem::Status status, const QString &errorString = QString()) override;

    /**
     * Aborts all running network jobs, except for the ones that mayAbortJob
     * returns false on and, for async aborts, emits abortFinished when done.
     */
    void abortNetworkJobs(
        AbortType abortType,
        const std::function<bool(AbstractNetworkJob *job)> &mayAbortJob);

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
    static void adjustLastJobTimeout(AbstractNetworkJob *job, qint64 fileSize);

    /** Bases headers that need to be sent on the PUT, or in the MOVE for chunking-ng */
    QMap<QByteArray, QByteArray> headers();

#ifdef Q_OS_WIN
    Utility::Handle m_fileLock;
#endif

private:
    bool _quotaUpdated = false;

    std::unordered_set<AbstractNetworkJob *> _childJobs; /// network jobs that are currently in transit
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
    uint _transferId = 0; /// transfer id (part of the url)

    qint64 chunkSize() const {
        // Old chunking does not use dynamic chunking algorithm, and does not adjusts the chunk size respectively,
        // thus this value should be used as the one classifing item to be chunked
        return propagator()->syncOptions()._initialChunkSize;
    }

public:
    PropagateUploadFileV1(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateUploadFileCommon(propagator, item)
    {
    }

    void doStartUpload() override;
public slots:
    void abort(PropagatorJob::AbortType abortType) override;
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
    /** Amount of data that was already sent in bytes.
     *
     * If this job is resuming an upload, this number includes bytes that were
     * sent in previous jobs.
     */
    qint64 _sent = 0;

    /** Amount of data that needs to be sent to the server in bytes.
     *
     * For normal uploads this will be the file size.
     *
     * This value is intended to be comparable to _sent: it's always the total
     * amount of data that needs to be present at the server to finish the upload -
     * regardless of whether previous jobs have already sent something.
     */
    qint64 _bytesToUpload;

    uint _transferId = 0; /// transfer id (part of the url)
    qint64 _currentChunkOffset = 0; /// byte offset of the next chunk data that will be sent
    qint64 _currentChunkSize = 0; /// current chunk size
    bool _removeJobError = false; /// if not null, there was an error removing the job

    // Map chunk number with its size  from the PROPFIND on resume.
    // (Only used from slotPropfindIterate/slotPropfindFinished because the LsColJob use signals to report data.)
    struct ServerChunkInfo
    {
        qint64 size;
        QString originalName;
    };
    QMap<qint64, ServerChunkInfo> _serverChunks;

    // Vector with expected PUT ranges.
    struct UploadRangeInfo
    {
        qint64 start;
        qint64 size;
        qint64 end() const { return start + size; }
    };
    QVector<UploadRangeInfo> _rangesToUpload;

    /**
     * Return the path of a chunk.
     * If chunkOffset == -1, returns the URL of the parent folder containing the chunks
     */
    QString chunkPath(qint64 chunkOffset = -1);

    /**
     * Finds the range starting at 'start' in _rangesToUpload and removes the first
     * 'size' bytes from it. If it becomes empty, remove the range.
     *
     * Retuns false if no matching range was found.
     */
    bool markRangeAsDone(qint64 start, qint64 size);

public:
    PropagateUploadFileNG(OwncloudPropagator *propagator, const SyncFileItemPtr &item);
    void doStartUpload() override;

private:
    void doStartUploadNext();
    void startNewUpload();
    void startNextChunk();
    void doFinalMove();
public slots:
    void abort(AbortType abortType) override;
private slots:
    void slotPropfindFinished();
    void slotPropfindFinishedWithError();
    void slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties);
    void slotDeleteJobFinished();
    void slotMkColFinished();
    void slotPutFinished();
    void slotMoveJobFinished();
    void slotUploadProgress(qint64, qint64);
};
}
