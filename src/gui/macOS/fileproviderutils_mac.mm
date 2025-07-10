/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderutils.h"

#include <QLoggingCategory>
#include <QString>

#import <FileProvider/FileProvider.h>

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

Q_LOGGING_CATEGORY(lcMacFileProviderUtils, "nextcloud.gui.macfileproviderutils", QtInfoMsg)

NSFileProviderDomain *domainForIdentifier(const QString &domainIdentifier)
{
    __block NSFileProviderDomain *foundDomain = nil;
    NSString *const nsDomainIdentifier = domainIdentifier.toNSString();
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

    // getDomainsWithCompletionHandler is asynchronous -- we create a dispatch semaphore in order
    // to wait until it is done. This should tell you that we should not call this method very
    // often!

    [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *const domains, NSError *const error) {
        if (error != nil) {
            qCWarning(lcMacFileProviderUtils) << "Error fetching domains:"
                                              << error.localizedDescription;
            dispatch_semaphore_signal(semaphore);
            return;
        }

        for (NSFileProviderDomain *const domain in domains) {
            if ([domain.identifier isEqualToString:nsDomainIdentifier]) {
                [domain retain];
                foundDomain = domain;
                break;
            }
        }

        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
    dispatch_release(semaphore);

    if (foundDomain == nil) {
        qCWarning(lcMacFileProviderUtils) << "No matching item domain for identifier"
                                          << domainIdentifier;
    }

    return foundDomain;
}

NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier)
{
    NSFileProviderDomain * const domain = domainForIdentifier(domainIdentifier);
    if (domain == nil) {
        qCWarning(lcMacFileProviderUtils) << "Received null domain for identifier"
                                          << domainIdentifier
                                          << "cannot acquire manager";
        return nil;
    }

    NSFileProviderManager * const manager = [NSFileProviderManager managerForDomain:domain];
    if (manager == nil) {
        qCWarning(lcMacFileProviderUtils) << "Received null manager for domain"
                                          << domainIdentifier;
    }

    [domain release];
    return manager;
}

QString groupContainerPath()
{
    NSString *const groupId = (NSString *)[NSBundle.mainBundle objectForInfoDictionaryKey:@"NCFPKAppGroupIdentifier"];
    if (groupId == nil) {
        qCWarning(lcMacFileProviderUtils) << "No app group identifier found in Info.plist, cannot determine group container path.";
        return QString();
    }
    return QString::fromNSString([NSFileManager.defaultManager containerURLForSecurityApplicationGroupIdentifier:groupId].path);
}

} // namespace FileProviderUtils

} // namespace Mac

} // namespace OCC
