/*
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    [[nodiscard]] bool atEnd() const override;
    [[nodiscard]] qint64 size() const override;
    [[nodiscard]] qint64 bytesAvailable() const override;
    [[nodiscard]] bool isSequential() const override;
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
    qint64 _bandwidthQuota = 0;
    qint64 _readWithProgress = 0;
    bool _bandwidthLimited = false; // if _bandwidthQuota will be used
    bool _choked = false; // if upload is paused (readData() will return 0)
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
    explicit PUTFileJob(AccountPtr account, const QString &path, std::unique_ptr<QIODevice> device,
        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject *parent = nullptr)
        : AbstractNetworkJob(account, path, parent)
        , _device(device.release())
        , _headers(headers)
        , _chunk(chunk)
    {
        _device->setParent(this);
    }
    explicit PUTFileJob(AccountPtr account, const QUrl &url, std::unique_ptr<QIODevice> device,
        const QMap<QByteArray, QByteArray> &headers, int chunk, QObject *parent = nullptr)
        : AbstractNetworkJob(account, QString(), parent)
        , _device(device.release())
        , _headers(headers)
        , _url(url)
        , _chunk(chunk)
    {
        _device->setParent(this);
    }
    ~PUTFileJob() override;

    int _chunk;

    void start() override;

    bool finished() override;

    QIODevice *device()
    {
        return _device;
    }

    [[nodiscard]] QString errorString() const override
    {
        return _errorString.isEmpty() ? AbstractNetworkJob::errorString() : _errorString;
    }

    [[nodiscard]] std::chrono::milliseconds msSinceStart() const
    {
        return std::chrono::milliseconds(_requestTimer.elapsed());
    }

signals:
    void finishedSignal();
    void uploadProgress(qint64, qint64);

};

/**
 * @brief This job implements the asynchronous PUT
 *
 * If the server replies to a PUT with a OC-JobStatus-Location path, we will query this url until the server
 * replies with an etag.
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

    void start() override;
    bool finished() override;

signals:
    void finishedSignal();
};

class PropagateUploadEncrypted;

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

    struct UploadStatus {
        SyncFileItem::Status status = SyncFileItem::NoStatus;
        QString message;
    };

protected:
    QVector<AbstractNetworkJob *> _jobs; /// network jobs that are currently in transit
    bool _finished BITFIELD(1); /// Tells that all the jobs have been finished
    bool _deleteExisting BITFIELD(1);

    /** Whether an abort is currently ongoing.
     *
     * Important to avoid duplicate aborts since each finishing PUTFileJob might
     * trigger an abort on error.
     */
    bool _aborting BITFIELD(1);

    /* This is a minified version of the SyncFileItem,
     * that holds only the specifics about the file that's
     * being uploaded.
     *
     * This is needed if we wanna apply changes on the file
     * that's being uploaded while keeping the original on disk.
     */
    struct UploadFileInfo {
      QString _file; /// I'm still unsure if I should use a SyncFilePtr here.
      QString _path; /// the full path on disk.
      qint64 _size = 0LL;
    };
    UploadFileInfo _fileToUpload;
    QByteArray _transmissionChecksumHeader;

public:
    PropagateUploadFileCommon(OwncloudPropagator *propagator, const SyncFileItemPtr &item);

    /**
     * Whether an existing entity with the same name may be deleted before
     * the upload.
     *
     * Default: false.
     */
    void setDeleteExisting(bool enabled);

    /* start should setup the file, path and size that will be send to the server */
    void start() override;
    void setupEncryptedFile(const QString& path, const QString& filename, quint64 size);
    void setupUnencryptedFile();
    void startUploadFile();
    void callUnlockFolder();
    bool isLikelyFinishedQuickly() override { return _item->_size < propagator()->smallFileSize(); }

private slots:
    void slotComputeContentChecksum();
    // Content checksum computed, compute the transmission checksum
    void slotComputeTransmissionChecksum(const QByteArray &contentChecksumType, const QByteArray &contentChecksum);
    // transmission checksum computed, prepare the upload
    void slotStartUpload(const QByteArray &transmissionChecksumType, const QByteArray &transmissionChecksum);
    // invoked when encrypted folder lock has been released
    void slotFolderUnlocked(const QByteArray &folderId, int httpReturnCode);
    // invoked on internal error to unlock a folder and failed
    void slotOnErrorStartFolderUnlock(OCC::SyncFileItem::Status status, const QString &errorString);

public:
    virtual void doStartUpload() = 0;

    void startPollJob(const QString &path);
    void finalize();
    void abortWithError(SyncFileItem::Status status, const QString &error);

public slots:
    void slotJobDestroyed(QObject *job);

private slots:
    void slotPollFinished();

protected:
    void done(const SyncFileItem::Status status, const QString &errorString = QString(), const ErrorCategory category = ErrorCategory::NoError) override;

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
private:
  PropagateUploadEncrypted *_uploadEncryptedHelper = nullptr;
  bool _uploadingEncrypted = false;
  UploadStatus _uploadStatus;
};

/**
 * @ingroup libsync
 *
 * Propagation job, implementing the old chunking algorithm
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

    [[nodiscard]] qint64 chunkSize() const {
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
    void abort(OCC::PropagatorJob::AbortType abortType) override;
private slots:
    void startNextChunk();
    void slotPutFinished();
    void slotUploadProgress(qint64, qint64);
};

/**
 * @ingroup libsync
 *
 * Propagation job, implementing the new chunking algorithm
 *
 */
class PropagateUploadFileNG : public PropagateUploadFileCommon
{
    Q_OBJECT

public:
    PropagateUploadFileNG(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateUploadFileCommon(propagator, item)
    {
    }

    void doStartUpload() override;

public slots:
    void abort(OCC::PropagatorJob::AbortType abortType) override;

private slots:
    void slotPropfindFinished();
    void slotPropfindFinishedWithError();
    void slotPropfindIterate(const QString &name, const QMap<QString, QString> &properties);
    void slotDeleteJobFinished();
    void slotMkColFinished();
    void slotPutFinished();
    void slotMoveJobFinished();
    void slotUploadProgress(qint64, qint64);

private:
    // Map chunk number with its size  from the PROPFIND on resume.
    // (Only used from slotPropfindIterate/slotPropfindFinished because the LsColJob use signals to report data.)
    struct ServerChunkInfo {
        qint64 size = 0LL;
        QString originalName;
    };

    [[nodiscard]] QUrl chunkUploadFolderUrl() const;
    [[nodiscard]] QUrl chunkUrl(const int chunk) const;
    [[nodiscard]] QByteArray destinationHeader() const;

    void startNewUpload();
    void startNextChunk();
    void finishUpload();

    QMap<qint64, ServerChunkInfo> _serverChunks;

    qint64 _sent = 0; /// amount of data (bytes) that was already sent
    uint _transferId = 0; /// transfer id (part of the url)
    int _currentChunk = 1; /// Id of the next chunk that will be sent
    qint64 _currentChunkSize = 0; /// current chunk size
    bool _removeJobError = false; /// If not null, there was an error removing the job
};
}
