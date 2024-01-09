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

#include "fileproviderdomainsyncstatus.h"

#include <QLoggingCategory>

#include "gui/macOS/fileproviderutils.h"

#import <FileProvider/FileProvider.h>

namespace OCC::Mac
{

Q_LOGGING_CATEGORY(lcMacFileProviderDomainSyncStatus, "nextcloud.gui.macfileproviderdomainsyncstatus", QtInfoMsg)

class FileProviderDomainSyncStatus::MacImplementation
{
public:
    explicit MacImplementation(const QString &domainIdentifier, FileProviderDomainSyncStatus *parent = nullptr)
        : q(parent)
    {
        _domain = FileProviderUtils::domainForIdentifier(domainIdentifier);
        _manager = [NSFileProviderManager managerForDomain:_domain];

        if (_manager == nil) {
            qCWarning(lcMacFileProviderDomainSyncStatus) << "Could not get manager for domain" << domainIdentifier;
            return;
        }

        _downloadProgress = [_manager globalProgressForKind:NSProgressFileOperationKindDownloading];
        _uploadProgress = [_manager globalProgressForKind:NSProgressFileOperationKindUploading];
    }

    ~MacImplementation() = default;

private:
    NSFileProviderDomain *_domain;
    NSFileProviderManager *_manager;
    NSProgress *_downloadProgress;
    NSProgress *_uploadProgress;
    QTimer _timer;
    FileProviderDomainSyncStatus *q;
};

FileProviderDomainSyncStatus::FileProviderDomainSyncStatus(const QString &domainIdentifier, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<MacImplementation>(domainIdentifier, this))
{
}

} // OCC::Mac
