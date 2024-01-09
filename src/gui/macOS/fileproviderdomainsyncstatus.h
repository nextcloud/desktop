/*
* Copyright (C) 2024 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include <QObject>

#pragma once

namespace OCC::Mac
{

class FileProviderDomainSyncStatus : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadingChanged)
    Q_PROPERTY(bool uploading READ uploading NOTIFY uploadingChanged)
    Q_PROPERTY(double fractionCompleted READ fractionCompleted NOTIFY fractionCompletedChanged)
    Q_PROPERTY(double downloadFractionCompleted READ downloadFractionCompleted NOTIFY downloadFractionCompletedChanged)
    Q_PROPERTY(double uploadFractionCompleted READ uploadFractionCompleted NOTIFY uploadFractionCompletedChanged)
    Q_PROPERTY(int downloadFileTotalCount READ downloadFileTotalCount NOTIFY downloadFileTotalCountChanged)
    Q_PROPERTY(int downloadFileCompletedCount READ downloadFileCompletedCount NOTIFY downloadFileCompletedCountChanged)
    Q_PROPERTY(int uploadFileTotalCount READ uploadFileTotalCount NOTIFY uploadFileTotalCountChanged)
    Q_PROPERTY(int uploadFileCompletedCount READ uploadFileCompletedCount NOTIFY uploadFileCompletedCountChanged)
    // TODO: more detailed reporting (time remaining, megabytes, etc.)

public:
    explicit FileProviderDomainSyncStatus(const QString &domainIdentifier, QObject *parent = nullptr);
    ~FileProviderDomainSyncStatus() override;

    bool syncing() const;
    bool downloading() const;
    bool uploading() const;
    double fractionCompleted() const;
    double downloadFractionCompleted() const;
    double uploadFractionCompleted() const;
    int downloadFileTotalCount() const;
    int downloadFileCompletedCount() const;
    int uploadFileTotalCount() const;
    int uploadFileCompletedCount() const;

signals:
    void syncingChanged(bool syncing);
    void downloadingChanged(bool downloading);
    void uploadingChanged(bool uploading);
    void fractionCompletedChanged(double fractionCompleted);
    void downloadFractionCompletedChanged(double downloadFractionCompleted);
    void uploadFractionCompletedChanged(double uploadFractionCompleted);
    void downloadFileTotalCountChanged(int downloadFileTotalCount);
    void downloadFileCompletedCountChanged(int downloadFileCompletedCount);
    void uploadFileTotalCountChanged(int uploadFileTotalCount);
    void uploadFileCompletedCountChanged(int uploadFileCompletedCount);

private:
    void setDownloading(const bool syncing);
    void setUploading(const bool syncing);
    void setDownloadFractionCompleted(const double fractionCompleted);
    void setUploadFractionCompleted(const double fractionCompleted);
    void setDownloadFileTotalCount(const int fileTotalCount);
    void setDownloadFileCompletedCount(const int fileCompletedCount);
    void setUploadFileTotalCount(const int fileTotalCount);
    void setUploadFileCompletedCount(const int fileCompletedCount);

    bool _downloading = false;
    bool _uploading = false;
    double _downloadFractionCompleted = 0.0;
    double _uploadFractionCompleted = 0.0;
    int _downloadFileTotalCount = 0;
    int _downloadFileCompletedCount = 0;
    int _uploadFileTotalCount = 0;
    int _uploadFileCompletedCount = 0;

    class MacImplementation;
    std::unique_ptr<MacImplementation> d;
};

} // OCC::Mac
