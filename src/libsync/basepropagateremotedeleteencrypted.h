/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QObject>
#include <QString>
#include <QNetworkReply>
#include "encryptedfoldermetadatahandler.h"
#include "syncfileitem.h"

namespace OCC {

class OwncloudPropagator;
/**
 * @brief The BasePropagateRemoteDeleteEncrypted class is the base class for Propagate Remote Delete Encrypted jobs
 * @ingroup libsync
 */
class BasePropagateRemoteDeleteEncrypted : public QObject
{
    Q_OBJECT
public:
    BasePropagateRemoteDeleteEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent);
    ~BasePropagateRemoteDeleteEncrypted() override = default;

    [[nodiscard]] QNetworkReply::NetworkError networkError() const;
    [[nodiscard]] QString errorString() const;

    virtual void start() = 0;

signals:
    void finished(bool success);

protected:
    void storeFirstError(QNetworkReply::NetworkError err);
    void storeFirstErrorString(const QString &errString);

    void fetchMetadataForPath(const QString &path);
    void uploadMetadata(const EncryptedFolderMetadataHandler::UploadMode uploadMode = EncryptedFolderMetadataHandler::UploadMode::DoNotKeepLock);

    [[nodiscard]] QSharedPointer<FolderMetadata> folderMetadata() const;
    [[nodiscard]] const QByteArray folderToken() const;

    void deleteRemoteItem(const QString &filename);

    void unlockFolder(const EncryptedFolderMetadataHandler::UnlockFolderWithResult result);
    void taskFailed();

protected slots:
    virtual void slotFolderUnLockFinished(const QByteArray &folderId, int statusCode);
    virtual void slotFetchMetadataJobFinished(int statusCode, const QString &message) = 0;
    virtual void slotUpdateMetadataJobFinished(int statusCode, const QString &message) = 0;
    void slotDeleteRemoteItemFinished();

protected:
    QPointer<OwncloudPropagator> _propagator = nullptr;
    SyncFileItemPtr _item;
    bool _isTaskFailed = false;
    QNetworkReply::NetworkError _networkError = QNetworkReply::NoError;
    QString _errorString;
    QString _fullFolderRemotePath;

private:
    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};

}
