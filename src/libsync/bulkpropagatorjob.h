/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "owncloudpropagator.h"
#include "abstractnetworkjob.h"

#include <QLoggingCategory>
#include <QVector>
#include <QMap>
#include <QByteArray>
#include <deque>

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcBulkPropagatorJob)

class ComputeChecksum;
class PutMultiFileJob;

class BulkPropagatorJob : public PropagatorJob
{
    Q_OBJECT

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

    struct BulkUploadItem
    {
        AccountPtr _account;
        SyncFileItemPtr _item;
        UploadFileInfo _fileToUpload;
        QString _remotePath;
        QString _localPath;
        qint64 _fileSize;
        QMap<QByteArray, QByteArray> _headers;
    };

public:
    explicit BulkPropagatorJob(OwncloudPropagator *propagator,
                               const std::deque<SyncFileItemPtr> &items);

    bool scheduleSelfOrChild() override;

    [[nodiscard]] JobParallelism parallelism() const override;

private slots:
    void startUploadFile(OCC::SyncFileItemPtr item, OCC::BulkPropagatorJob::UploadFileInfo fileToUpload);

    // Content checksum computed, compute the transmission checksum
    void slotComputeTransmissionChecksum(OCC::SyncFileItemPtr item,
                                         OCC::BulkPropagatorJob::UploadFileInfo fileToUpload);

    void slotComputeMd5Checksum(OCC::SyncFileItemPtr item,
                                OCC::BulkPropagatorJob::UploadFileInfo fileToUpload,
                                const QByteArray &transmissionChecksumType,
                                const QByteArray &transmissionChecksum);

    // transmission checksum computed, prepare the upload
    void slotStartUpload(OCC::SyncFileItemPtr item,
                         OCC::BulkPropagatorJob::UploadFileInfo fileToUpload,
                         const QByteArray &transmissionChecksumType,
                         const QByteArray &transmissionChecksum,
                         const QByteArray &md5Checksum);

    // invoked on internal error to unlock a folder and failed
    void slotOnErrorStartFolderUnlock(OCC::SyncFileItemPtr item,
                                      const OCC::SyncFileItem::Status status,
                                      const QString &errorString,
                                      const OCC::ErrorCategory errorCategory);

    void slotPutFinished();

    void slotUploadProgress(OCC::SyncFileItemPtr item, qint64 sent, qint64 total);

    void slotJobDestroyed(QObject *job);

private:
    void doStartUpload(SyncFileItemPtr item,
                       UploadFileInfo fileToUpload,
                       QByteArray transmissionChecksumHeader,
                       QByteArray md5ChecksumHeader);

    void adjustLastJobTimeout(AbstractNetworkJob *job,
                              qint64 fileSize) const;

    void finalize(const QJsonObject &fullReply);

    void finalizeOneFile(const BulkUploadItem &oneFile);

    void slotPutFinishedOneFile(const BulkUploadItem &singleFile,
                                OCC::PutMultiFileJob *job,
                                const QJsonObject &fullReplyObject);

    void done(SyncFileItemPtr item,
              const SyncFileItem::Status status,
              const QString &errorString,
              const ErrorCategory category);

    /** Bases headers that need to be sent on the PUT, or in the MOVE for chunking-ng */
    [[nodiscard]] QMap<QByteArray, QByteArray> headers(SyncFileItemPtr item) const;

    void abortWithError(SyncFileItemPtr item,
                        SyncFileItem::Status status,
                        const QString &error);

    /**
     * Checks whether the current error is one that should reset the whole
     * transfer if it happens too often. If so: Bump UploadInfo::errorCount
     * and maybe perform the reset.
     */
    void checkResettingErrors(SyncFileItemPtr item) const;

    /**
     * Error handling functionality that is shared between jobs.
     */
    void commonErrorHandling(SyncFileItemPtr item,
                             const QString &errorMessage);

    bool checkFileStillExists(SyncFileItemPtr item,
                              const bool finished,
                              const QString &fullFilePath);

    bool checkFileChanged(SyncFileItemPtr item,
                          const bool finished,
                          const QString &fullFilePath);

    void computeFileId(SyncFileItemPtr item,
                       const QJsonObject &fileReply) const;

    void handleFileRestoration(SyncFileItemPtr item,
                               const QString &errorString) const;

    void handleBulkUploadBlackList(SyncFileItemPtr item) const;

    void handleJobDoneErrors(SyncFileItemPtr item,
                             SyncFileItem::Status status);

    void triggerUpload();

    void checkPropagationIsDone();

    bool handleBatchSize();

    std::deque<SyncFileItemPtr> _items;

    QVector<AbstractNetworkJob *> _jobs; /// network jobs that are currently in transit

    QSet<QString> _pendingChecksumFiles;

    std::vector<BulkUploadItem> _filesToUpload;

    qint64 _sentTotal = 0;

    SyncFileItem::Status _finalStatus = SyncFileItem::Status::NoStatus;
    int _currentBatchSize = 0;
};

}
