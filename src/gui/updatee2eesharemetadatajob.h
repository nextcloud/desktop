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


#include "account.h"
#include "folder.h"
#include "sharee.h"
#include "sharemanager.h"

#include <QHash>
#include <QObject>
#include <QSslCertificate>
#include <QString>

namespace OCC {
class FolderMetadata;
class UpdateE2eeShareMetadataJob : public QObject
{
    Q_OBJECT

public:
    enum Operation { Invalid = -1, Add = 0, Remove, ReEncrypt };
    explicit UpdateE2eeShareMetadataJob(const AccountPtr &account,
                                const QByteArray &folderId,
                                const QString &folderAlias,
                                const ShareePtr &sharee,
                                const Operation operation,
                                const QString &sharePath = {},
                                const Share::Permissions desiredPermissions = {},
                                const QString &password = {},
                                QObject *parent = nullptr);

public:
    [[nodiscard]] QString password() const;
    [[nodiscard]] QString sharePath() const;
    [[nodiscard]] Share::Permissions desiredPermissions() const;
    [[nodiscard]] ShareePtr sharee() const;

public slots:
    void start();
    void setTopLevelFolderMetadata(const QSharedPointer<FolderMetadata> &topLevelFolderMetadata);
    void setFolderToken(const QByteArray &folderToken);
    void setMetadataKeyForDecryption(const QByteArray &metadataKey);

private slots:
    void slotCertificatesFetchedFromServer(const QHash<QString, QSslCertificate> &results);
    void slotCertificateFetchedFromKeychain(const QSslCertificate certificate);
    void slotCertificateReady(const QSslCertificate certificate);
    void slotFetchFolderMetadata();
    void slotMetadataReceived(const QJsonDocument &json, int statusCode);
    void slotMetadataError(const QByteArray &folderId, int httpReturnCode);
    void slotScheduleSubJobs();
    void slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token);
    void slotFolderLockedError(const QByteArray &folderId, int httpErrorCode);
    void slotLockFolder();
    void slotUnlockFolder();
    void slotUpdateMetadataSuccess(const QByteArray &folderId);
    void slotUpdateMetadataError(const QByteArray &folderId, int httpReturnCode);
    void slotFolderUnlocked(const QByteArray &folderId, int httpStatus);
    void slotUpdateFolderMetadata();
    void slotSubJobsFinished();
    void slotSubJobFinished(int code, const QString &message = {});

private: signals:
    void certificateReady(QSslCertificate certificate);
    void finished(int code, const QString &message = {});

private:
    AccountPtr _account;
    QByteArray _folderId;
    QString _folderAlias;
    ShareePtr _sharee;
    Operation _operation;
    QString _sharePath;
    Share::Permissions _desiredPermissions = {};
    QString _password;
    QSslCertificate _shareeCertificate;
    QByteArray _folderToken;
    QByteArray _metadataKeyForDecryption;
    QSharedPointer<FolderMetadata> _folderMetadata;
    QSet<UpdateE2eeShareMetadataJob *> _subJobs;
    QSharedPointer<FolderMetadata> _topLevelFolderMetadata;
    QPointer<Folder> _folder;
};

}
