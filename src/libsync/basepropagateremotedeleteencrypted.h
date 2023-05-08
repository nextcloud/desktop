/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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
