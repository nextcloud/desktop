/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include "owncloudlib.h"
#include "owncloudpropagator.h"
#include "networkjobs.h"
#include "clientsideencryption.h"
#include <common/checksums.h>
#include "foldermetadata.h"

#include <QBuffer>
#include <QFile>

#include <filesystem>

namespace OCC {
class PropagateDownloadEncrypted;

/**
 * @brief The GETFileJob class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT GETFileJob : public AbstractNetworkJob
{
    Q_OBJECT
    QIODevice *_device;
    QMap<QByteArray, QByteArray> _headers;
    QString _errorString;
    QByteArray _expectedEtagForResume;
    qint64 _expectedContentLength;
    qint64 _resumeStart;
    SyncFileItem::Status _errorStatus;
    QUrl _directDownloadUrl;
    QByteArray _etag;
    bool _bandwidthLimited; // if _bandwidthQuota will be used
    bool _bandwidthChoked; // if download is paused (won't read on readyRead())
    qint64 _bandwidthQuota;
    QPointer<BandwidthManager> _bandwidthManager;
    bool _hasEmittedFinishedSignal;
    time_t _lastModified;

    /// Will be set to true once we've seen a 2xx response header
    bool _saveBodyToFile = false;
    qint64 _decompressionThresholdBase = 0;

protected:
    qint64 _contentLength;

public:
    // DOES NOT take ownership of the device.
    explicit GETFileJob(AccountPtr account, const QString &path, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
        qint64 resumeStart, QObject *parent = nullptr);
    // For directDownloadUrl:
    explicit GETFileJob(AccountPtr account, const QUrl &url, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
        qint64 resumeStart, QObject *parent = nullptr);
    ~GETFileJob() override
    {
        if (_bandwidthManager) {
            _bandwidthManager->unregisterDownloadJob(this);
        }
    }

    void start() override;
    bool finished() override
    {
        if (_saveBodyToFile && reply()->bytesAvailable()) {
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

    void cancel();

    void newReplyHook(QNetworkReply *reply) override;

    void setBandwidthManager(BandwidthManager *bwm);
    void setChoked(bool c);
    void setBandwidthLimited(bool b);
    void giveBandwidthQuota(qint64 q);
    qint64 currentDownloadPosition();

    [[nodiscard]] QString errorString() const override;
    void setErrorString(const QString &s) { _errorString = s; }

    SyncFileItem::Status errorStatus() { return _errorStatus; }
    void setErrorStatus(const SyncFileItem::Status &s) { _errorStatus = s; }

    void onTimedOut() override;

    QByteArray &etag() { return _etag; }
    qint64 resumeStart() { return _resumeStart; }
    time_t lastModified() { return _lastModified; }

    [[nodiscard]] qint64 contentLength() const { return _contentLength; }
    [[nodiscard]] qint64 expectedContentLength() const { return _expectedContentLength; }
    void setExpectedContentLength(qint64 size) { _expectedContentLength = size; }

    [[nodiscard]] qint64 decompressionThresholdBase() const { return _decompressionThresholdBase; }
    void setDecompressionThresholdBase(qint64 decompressionThresholdBase) { _decompressionThresholdBase = decompressionThresholdBase; }

protected:
    virtual qint64 writeToDevice(const QByteArray &data);

signals:
    void finishedSignal();
    void downloadProgress(qint64, qint64);
private slots:
    void slotReadyRead();
    void slotMetaDataChanged();
};

/**
 * @brief The GETEncryptedFileJob class that provides file decryption on the fly while the download is running
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT GETEncryptedFileJob : public GETFileJob
{
    Q_OBJECT

public:
    // DOES NOT take ownership of the device.
    explicit GETEncryptedFileJob(AccountPtr account, const QString &path, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
        qint64 resumeStart, FolderMetadata::EncryptedFile encryptedInfo, QObject *parent = nullptr);
    explicit GETEncryptedFileJob(AccountPtr account, const QUrl &url, QIODevice *device,
        const QMap<QByteArray, QByteArray> &headers, const QByteArray &expectedEtagForResume,
        qint64 resumeStart, FolderMetadata::EncryptedFile encryptedInfo, QObject *parent = nullptr);
    ~GETEncryptedFileJob() override = default;

protected:
    qint64 writeToDevice(const QByteArray &data) override;

private:
    QSharedPointer<EncryptionHelper::StreamingDecryptor> _decryptor;
    FolderMetadata::EncryptedFile _encryptedFileInfo = {};
    QByteArray _pendingBytes;
    qint64 _processedSoFar = 0;
};

/**
 * @brief The PropagateDownloadFile class
 * @ingroup libsync
 *
 * This is the flow:

\code{.unparsed}
  start()
    |
    | deleteExistingFolder() if enabled
    |
    +--> mtime and size identical?
    |    then compute the local checksum
    |                               done?-> conflictChecksumComputed()
    |                                              |
    |                         checksum differs?    |
    +-> startDownload() <--------------------------+
          |                                        |
          +-> run a GETFileJob                     | checksum identical?
                                                   |
      done?-> slotGetFinished()                    |
                |                                  |
                +-> validate checksum header       |
                                                   |
      done?-> transmissionChecksumValidated()      |
                |                                  |
                +-> compute the content checksum   |
                                                   |
      done?-> contentChecksumComputed()            |
                |                                  |
                +-> downloadFinished()             |
                       |                           |
    +------------------+                           |
    |                                              |
    +-> updateMetadata() <-------------------------+

\endcode
 */
class PropagateDownloadFile : public PropagateItemJob
{
    Q_OBJECT
public:
    PropagateDownloadFile(OwncloudPropagator *propagator, const SyncFileItemPtr &item)
        : PropagateItemJob(propagator, item)
    {
    }
    void start() override;
    [[nodiscard]] qint64 committedDiskSpace() const override;

    // We think it might finish quickly because it is a small file.
    bool isLikelyFinishedQuickly() override { return _item->_size < propagator()->smallFileSize(); }

    /**
     * Whether an existing folder with the same name may be deleted before
     * the download.
     *
     * If it's a non-empty folder, it'll be renamed to a conflict-style name
     * to preserve any non-synced content that may be inside.
     *
     * Default: false.
     */
    void setDeleteExistingFolder(bool enabled);

protected:
    void done(const SyncFileItem::Status status, const QString &errorString, const ErrorCategory category) override;

    void makeParentFolderModifiable(const QString &fileName);

private slots:
    /// Called when ComputeChecksum on the local file finishes,
    /// maybe the local and remote checksums are identical?
    void conflictChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum);
    /// Called to start downloading the remote file
    void startDownload();
    /// Called when the GETFileJob finishes
    void slotGetFinished();
    /// Called when the download's checksum header was validated
    void transmissionChecksumValidated(const QByteArray &checksumType, const QByteArray &checksum);
    /// Called when the download's checksum computation is done
    void contentChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum);
    /// Called when the local file's checksum computation is done
    void localFileContentChecksumComputed(const QByteArray &checksumType, const QByteArray &checksum);

    void finalizeDownload();
    void downloadFinished();
    /// Called when it's time to update the db metadata
    void updateMetadata(bool isConflict);

    void abort(OCC::PropagatorJob::AbortType abortType) override;
    void slotDownloadProgress(qint64, qint64);
    void slotChecksumFail(const QString &errMsg, const QByteArray &calculatedChecksumType,
        const QByteArray &calculatedChecksum, const OCC::ValidateChecksumHeader::FailureReason reason);
    void processChecksumRecalculate(const QNetworkReply *reply, const QByteArray &originalChecksumHeader, const QString &errorMessage);
    void checksumValidateFailedAbortDownload(const QString &errMsg);

private:
    void startAfterIsEncryptedIsChecked();
    void deleteExistingFolder();
    [[nodiscard]] bool isEncrypted() const { return _isEncrypted; }

    qint64 _resumeStart = 0;
    qint64 _downloadProgress = 0;
    QPointer<GETFileJob> _job;
    QFile _tmpFile;
    bool _deleteExisting = false;
    bool _isEncrypted = false;
    FolderMetadata::EncryptedFile _encryptedInfo;
    ConflictRecord _conflictRecord;

    QElapsedTimer _stopwatch;

    PropagateDownloadEncrypted *_downloadEncryptedHelper = nullptr;

    std::filesystem::path _parentPath;
    bool _needParentFolderRestorePermissions = false;
};
}
