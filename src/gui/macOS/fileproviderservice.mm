/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderservice.h"

#import <Foundation/Foundation.h>
#import "AppProtocol.h"

#include <QLoggingCategory>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMetaObject>

#include "accountmanager.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcMacFileProviderService, "nextcloud.gui.macfileproviderservice", QtInfoMsg)

} // namespace OCC

/**
 * @brief Objective-C delegate that implements the AppProtocol.
 */
@interface FileProviderServiceDelegate : NSObject<AppProtocol>
@property (nonatomic, assign) OCC::Mac::FileProviderService *service;
@end

@implementation FileProviderServiceDelegate

- (void)presentFileActions:(NSString *)fileId path:(NSString *)path remoteItemPath:(NSString *)remoteItemPath withDomainIdentifier:(NSString *)domainIdentifier
{
    qCDebug(OCC::lcMacFileProviderService) << "Should present file actions for item with fileId:"
                                           << fileId
                                           << "and path:"
                                           << path
                                           << "remote item path:"
                                           << remoteItemPath
                                           << "domain identifier:"
                                           << domainIdentifier;

    const auto qFileId = QString::fromNSString(fileId);
    const auto localPath = QString::fromNSString(path);
    const auto qRemoteItemPath = QString::fromNSString(remoteItemPath);
    const auto domainId = QString::fromNSString(domainIdentifier);
    
    // Use QMetaObject::invokeMethod to emit the signal on the correct thread
    // since this callback may be called on an XPC dispatch queue (non-main thread)
    QMetaObject::invokeMethod(_service, "showFileActionsDialog", Qt::QueuedConnection,
                              Q_ARG(QString, qFileId),
                              Q_ARG(QString, localPath),
                              Q_ARG(QString, qRemoteItemPath),
                              Q_ARG(QString, domainId));
}

- (void)reportSyncStatus:(NSString *)status forDomainIdentifier:(NSString *)domainIdentifier
{
    const auto statusString = QString::fromNSString(status);
    const auto domainIdString = QString::fromNSString(domainIdentifier);
    
    qCDebug(OCC::lcMacFileProviderService) << "Received sync status from file provider extension:"
                                           << statusString
                                           << "for domain:" << domainIdString;
    
    if (!_service) {
        qCWarning(OCC::lcMacFileProviderService) << "No service available to report sync state";
        return;
    }
    
    const auto accountState = OCC::AccountManager::instance()->accountFromFileProviderDomainIdentifier(domainIdString);
    if (!accountState) {
        qCWarning(OCC::lcMacFileProviderService) << "No account state found for domain identifier:" << domainIdString;
        return;
    }
    
    const auto account = accountState->account();
    if (!account) {
        qCWarning(OCC::lcMacFileProviderService) << "No account found for domain identifier:" << domainIdString;
        return;
    }
    
    auto syncState = OCC::SyncResult::Status::Undefined;
    if (statusString == QStringLiteral("SYNC_PREPARING")) {
        syncState = OCC::SyncResult::Status::SyncPrepare;
    } else if (statusString == QStringLiteral("SYNC_STARTED")) {
        syncState = OCC::SyncResult::Status::SyncRunning;
    } else if (statusString == QStringLiteral("SYNC_FINISHED")) {
        syncState = OCC::SyncResult::Status::Success;
    } else if (statusString == QStringLiteral("SYNC_FAILED")) {
        syncState = OCC::SyncResult::Status::Problem;
    } else if (statusString == QStringLiteral("SYNC_PAUSED")) {
        syncState = OCC::SyncResult::Status::Paused;
    } else {
        qCWarning(OCC::lcMacFileProviderService) << "Unknown sync state received:" << statusString;
    }
    
    const auto userId = account->userIdAtHostWithPort();
    qCDebug(OCC::lcMacFileProviderService) << "Received sync state change for account" << userId << "state" << syncState;
    _service->setLatestReceivedSyncStatus(userId, syncState);
    
    // Use QMetaObject::invokeMethod to emit the signal on the correct thread
    // since this callback may be called on an XPC dispatch queue (non-main thread)
    QMetaObject::invokeMethod(_service, "syncStateChanged", Qt::QueuedConnection,
                              Q_ARG(OCC::AccountPtr, account),
                              Q_ARG(OCC::SyncResult::Status, syncState));
}

@end

namespace OCC {

namespace Mac {

class FileProviderService::MacImplementation
{
public:
    FileProviderServiceDelegate *delegate = nil;
    
    MacImplementation()
    {
        qCDebug(lcMacFileProviderService) << "Initializing file provider service";
    }
    
    ~MacImplementation()
    {
        [delegate release];
        delegate = nil;
    }
};

FileProviderService::FileProviderService(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<MacImplementation>())
{
    qCDebug(lcMacFileProviderService) << "FileProviderService created";
    
    d->delegate = [[FileProviderServiceDelegate alloc] init];
    d->delegate.service = this;
}

FileProviderService::~FileProviderService()
{
    qCDebug(lcMacFileProviderService) << "FileProviderService destroyed";
}

void *FileProviderService::delegate() const
{
    return d ? d->delegate : nullptr;
}

SyncResult::Status FileProviderService::latestReceivedSyncStatusForAccount(const AccountPtr &account) const
{
    Q_ASSERT(account);
    QReadLocker locker(&_syncStatusLock);
    return _latestReceivedSyncStatus.value(account->userIdAtHostWithPort(), SyncResult::Undefined);
}

void FileProviderService::setLatestReceivedSyncStatus(const QString &userId, SyncResult::Status status)
{
    QWriteLocker locker(&_syncStatusLock);
    _latestReceivedSyncStatus.insert(userId, status);
}

} // namespace Mac

} // namespace OCC

