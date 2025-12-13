/*
 * SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileprovidereditlocallyjob.h"

#include <QLoggingCategory>

#include "account.h"
#include "accountstate.h"
#include "editlocallymanager.h"
#include "systray.h"

#include "macOS/fileprovider.h"
#include "macOS/fileproviderdomainmanager.h"

#import <Cocoa/Cocoa.h>
#import <FileProvider/FileProvider.h>

namespace OCC::Mac {

Q_LOGGING_CATEGORY(lcFileProviderEditLocallyMacJob, 
                   "nextcloud.gui.fileprovidereditlocallymac",
                   QtInfoMsg)

void FileProviderEditLocallyJob::openFileProviderFile(const QString &ocId)
{
    qCDebug(lcFileProviderEditLocallyMacJob) << "Opening file provider file with OC ID" << ocId;
    
    const auto nsOcId = ocId.toNSString();
    const auto userId = _accountState->account()->userIdAtHostWithPort();
    const auto ncDomainManager = FileProvider::instance()->domainManager();
    const auto account = _accountState ? _accountState->account() : AccountPtr{};
    const auto voidDomain = ncDomainManager->domainForAccount(account.data());
    
    NSFileProviderDomain *const domain = (NSFileProviderDomain *)voidDomain;
    
    if (domain == nil) {
        qCWarning(lcFileProviderEditLocallyMacJob) << "Could not get domain for account:" << userId;
        emit notAvailable();
        return;
    }

    NSFileProviderManager *const manager = [NSFileProviderManager managerForDomain:domain];

    if (manager == nil) {
        qCWarning(lcFileProviderEditLocallyMacJob) << "Could not get file provider manager"
                                                      "for domain of account:" << userId;;
        emit notAvailable();
        return;
    }

    [manager retain];
    [manager getUserVisibleURLForItemIdentifier:nsOcId
                              completionHandler:^(NSURL *const url, NSError *const error) {

        dispatch_async(dispatch_get_main_queue(), ^{
            Systray::instance()->destroyEditFileLocallyLoadingDialog();
        });

        if (error != nil) {
            const auto errorMessage = QString::fromNSString(error.localizedDescription);
            qCWarning(lcFileProviderEditLocallyMacJob) << "Error getting user visible URL for item:" << errorMessage;
            dispatch_async(dispatch_get_main_queue(), ^{
                emit notAvailable();
            });
        } else if (url != nil) {
            const auto itemLocalPath = QString::fromNSString(url.path);
            qCDebug(lcFileProviderEditLocallyMacJob) << "Got user visible URL for item:" << itemLocalPath;
            [NSWorkspace.sharedWorkspace openURL:url];
            dispatch_async(dispatch_get_main_queue(), ^{
                emit finished();
            });
        } else {
            qCWarning(lcFileProviderEditLocallyMacJob) << "Got nil user visible URL for item" << ocId;
            dispatch_async(dispatch_get_main_queue(), ^{
                emit notAvailable();
            });
        }
        [manager release];
    }];
}

} // namespace OCC::Mac
