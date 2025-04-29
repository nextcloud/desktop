/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma once

#include <QObject>

#include "account.h"

class QLocalServer;
class QLocalSocket;

namespace OCC {
class EncryptedFolderMetadataHandler;
class GETFileJob;
class SyncJournalDb;
class VfsCfApi;

namespace EncryptionHelper {
    class StreamingDecryptor;
};

class HydrationJob : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Success = 0,
        Error,
        Cancelled,
    };
    Q_ENUM(Status)

    explicit HydrationJob(QObject *parent = nullptr);

    ~HydrationJob() override;

    AccountPtr account() const;
    void setAccount(const AccountPtr &account);

    [[nodiscard]] QString remoteSyncRootPath() const;
    void setRemoteSyncRootPath(const QString &path);

    QString localPath() const;
    void setLocalPath(const QString &localPath);

    SyncJournalDb *journal() const;
    void setJournal(SyncJournalDb *journal);

    QString requestId() const;
    void setRequestId(const QString &requestId);

    QString folderPath() const;
    void setFolderPath(const QString &folderPath);

    bool isEncryptedFile() const;
    void setIsEncryptedFile(bool isEncrypted);

    QString e2eMangledName() const;
    void setE2eMangledName(const QString &e2eMangledName);

    qint64 fileTotalSize() const;
    void setFileTotalSize(qint64 totalSize);

    Status status() const;

    [[nodiscard]] int errorCode() const;
    [[nodiscard]] int statusCode() const;
    [[nodiscard]] QString errorString() const;

    void start();
    void cancel();
    void finalize(OCC::VfsCfApi *vfs);

signals:
    void finished(HydrationJob *job);

private slots:
    void slotFetchMetadataJobFinished(int statusCode, const QString &message);

private:
    void emitFinished(Status status);

    void onNewConnection();
    void onCancellationServerNewConnection();
    void onGetFinished();

    void handleNewConnection();
    void handleNewConnectionForEncryptedFile();

    void startServerAndWaitForConnections();

    AccountPtr _account;
    QString _remoteSyncRootPath;
    QString _localPath;
    SyncJournalDb *_journal = nullptr;
    bool _isCancelled = false;

    QString _requestId;
    QString _folderPath;

    bool _isEncryptedFile = false;
    QString _e2eMangledName;

    QLocalServer *_transferDataServer = nullptr;
    QLocalServer *_signalServer = nullptr;
    QLocalSocket *_transferDataSocket = nullptr;
    QLocalSocket *_signalSocket = nullptr;
    GETFileJob *_job = nullptr;
    Status _status = Success;
    int _errorCode = 0;
    int _statusCode = 0;
    QString _errorString;
    QString _remoteParentPath;

    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

} // namespace OCC
