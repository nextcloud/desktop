/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderdomainsyncstatus.h"

#include <QLoggingCategory>

#include "gui/macOS/fileproviderutils.h"
#include "libsync/theme.h"

#import <FileProvider/FileProvider.h>

#import "gui/macOS/progressobserver.h"

namespace OCC::Mac
{

Q_LOGGING_CATEGORY(lcMacFileProviderDomainSyncStatus, "nextcloud.gui.macfileproviderdomainsyncstatus", QtInfoMsg)

class FileProviderDomainSyncStatus::MacImplementation
{
public:
    explicit MacImplementation(const QString &domainIdentifier, FileProviderDomainSyncStatus *parent)
        : q(parent)
    {
        _domain = FileProviderUtils::domainForIdentifier(domainIdentifier);
        _manager = [NSFileProviderManager managerForDomain:_domain];

        if (_manager == nil) {
            qCWarning(lcMacFileProviderDomainSyncStatus) << "Could not get manager for domain" << domainIdentifier;
            return;
        }
        [_manager retain];

        if (@available(macOS 11.3, *)) {
            NSProgress *const downloadProgress = [_manager globalProgressForKind:NSProgressFileOperationKindDownloading];
            NSProgress *const uploadProgress = [_manager globalProgressForKind:NSProgressFileOperationKindUploading];
            _downloadProgressObserver = [[ProgressObserver alloc] initWithProgress:downloadProgress];
            _uploadProgressObserver = [[ProgressObserver alloc] initWithProgress:uploadProgress];

            _downloadProgressObserver.progressKVOChangeHandler = ^(NSProgress *const progress){
                updateDownload(progress);
            };
            _uploadProgressObserver.progressKVOChangeHandler = ^(NSProgress *const progress){
                updateUpload(progress);
            };
        }
    }

    ~MacImplementation()
    {
        [_downloadProgressObserver release];
        [_uploadProgressObserver release];
        [_domain release];
        [_manager release];
    }

    void updateDownload(NSProgress *const progress) const
    {
        qCInfo(lcMacFileProviderDomainSyncStatus) << "Download progress changed" << progress.localizedDescription;
        if (progress == nil || q == nullptr) {
            return;
        }

        q->setDownloading(!progress.paused && !progress.cancelled && !progress.finished);
        q->setDownloadFractionCompleted(progress.fractionCompleted);
        q->setDownloadFileTotalCount(progress.fileTotalCount.intValue);
        q->setDownloadFileCompletedCount(progress.fileCompletedCount.intValue);
        q->updateIcon();
    }

    void updateUpload(NSProgress *const progress) const
    {
        qCInfo(lcMacFileProviderDomainSyncStatus) << "Upload progress changed" << progress.localizedDescription;
        if (progress == nil || q == nullptr) {
            return;
        }

        q->setUploading(!progress.paused && !progress.cancelled && !progress.finished);
        q->setUploadFractionCompleted(progress.fractionCompleted);
        q->setUploadFileTotalCount(progress.fileTotalCount.intValue);
        q->setUploadFileCompletedCount(progress.fileCompletedCount.intValue);
        q->updateIcon();
    }

private:
    NSFileProviderDomain *_domain = nil;
    NSFileProviderManager *_manager = nil;
    ProgressObserver *_downloadProgressObserver = nullptr;
    ProgressObserver *_uploadProgressObserver = nullptr;
    FileProviderDomainSyncStatus *q = nullptr;
};

FileProviderDomainSyncStatus::FileProviderDomainSyncStatus(const QString &domainIdentifier, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<MacImplementation>(domainIdentifier, this))
{
    qRegisterMetaType<FileProviderDomainSyncStatus*>("FileProviderDomainSyncStatus*");
    updateIcon();
}

FileProviderDomainSyncStatus::~FileProviderDomainSyncStatus() = default;

bool FileProviderDomainSyncStatus::syncing() const
{
    return downloading() || uploading();
}

bool FileProviderDomainSyncStatus::downloading() const
{
    return _downloading;
}

bool FileProviderDomainSyncStatus::uploading() const
{
    return _uploading;
}

double FileProviderDomainSyncStatus::fractionCompleted() const
{
    return (downloadFractionCompleted() + uploadFractionCompleted()) / 2;
}

double FileProviderDomainSyncStatus::downloadFractionCompleted() const
{
    return _downloadFractionCompleted;
}

double FileProviderDomainSyncStatus::uploadFractionCompleted() const
{
    return _uploadFractionCompleted;
}

int FileProviderDomainSyncStatus::downloadFileTotalCount() const
{
    return _downloadFileTotalCount;
}

int FileProviderDomainSyncStatus::downloadFileCompletedCount() const
{
    return _downloadFileCompletedCount;
}

int FileProviderDomainSyncStatus::uploadFileTotalCount() const
{
    return _uploadFileTotalCount;
}

int FileProviderDomainSyncStatus::uploadFileCompletedCount() const
{
    return _uploadFileCompletedCount;
}

QUrl FileProviderDomainSyncStatus::icon() const
{
    return _icon;
}

void FileProviderDomainSyncStatus::setDownloading(const bool downloading)
{
    if (_downloading == downloading) {
        return;
    }

    _downloading = downloading;
    emit downloadingChanged(_downloading);
    emit syncingChanged(syncing());
}

void FileProviderDomainSyncStatus::setUploading(const bool uploading)
{
    if (_uploading == uploading) {
        return;
    }

    _uploading = uploading;
    emit uploadingChanged(_uploading);
    emit syncingChanged(syncing());
}

void FileProviderDomainSyncStatus::setDownloadFractionCompleted(const double downloadFractionCompleted)
{
    if (_downloadFractionCompleted == downloadFractionCompleted) {
        return;
    }

    _downloadFractionCompleted = downloadFractionCompleted;
    emit downloadFractionCompletedChanged(_downloadFractionCompleted);
    emit fractionCompletedChanged(fractionCompleted());
}

void FileProviderDomainSyncStatus::setUploadFractionCompleted(const double uploadFractionCompleted)
{
    if (_uploadFractionCompleted == uploadFractionCompleted) {
        return;
    }

    _uploadFractionCompleted = uploadFractionCompleted;
    emit uploadFractionCompletedChanged(_uploadFractionCompleted);
    emit fractionCompletedChanged(fractionCompleted());
}

void FileProviderDomainSyncStatus::setDownloadFileTotalCount(const int fileTotalCount)
{
    if (_downloadFileTotalCount == fileTotalCount) {
        return;
    }

    _downloadFileTotalCount = fileTotalCount;
    emit downloadFileTotalCountChanged(_downloadFileTotalCount);
}

void FileProviderDomainSyncStatus::setDownloadFileCompletedCount(const int fileCompletedCount)
{
    if (_downloadFileCompletedCount == fileCompletedCount) {
        return;
    }

    _downloadFileCompletedCount = fileCompletedCount;
    emit downloadFileCompletedCountChanged(_downloadFileCompletedCount);
}

void FileProviderDomainSyncStatus::setUploadFileTotalCount(const int fileTotalCount)
{
    if (_uploadFileTotalCount == fileTotalCount) {
        return;
    }

    _uploadFileTotalCount = fileTotalCount;
    emit uploadFileTotalCountChanged(_uploadFileTotalCount);
}

void FileProviderDomainSyncStatus::setUploadFileCompletedCount(const int fileCompletedCount)
{
    if (_uploadFileCompletedCount == fileCompletedCount) {
        return;
    }

    _uploadFileCompletedCount = fileCompletedCount;
    emit uploadFileCompletedCountChanged(_uploadFileCompletedCount);
}

void FileProviderDomainSyncStatus::setIcon(const QUrl &icon)
{
    if (_icon == icon) {
        return;
    }

    _icon = icon;
    emit iconChanged(_icon);
}

void FileProviderDomainSyncStatus::updateIcon()
{
    const auto iconUrl = syncing() ? Theme::instance()->sync() : Theme::instance()->ok();
    setIcon(iconUrl);
}

} // OCC::Mac
