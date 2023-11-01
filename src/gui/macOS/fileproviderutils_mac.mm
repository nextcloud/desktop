/*
 * Copyright 2023 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
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

#include "fileproviderutils.h"

#include <QLoggingCategory>
#include <QString>

#import <FileProvider/FileProvider.h>

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

Q_LOGGING_CATEGORY(lcMacFileProviderUtils, "nextcloud.gui.macfileproviderutils", QtInfoMsg)

NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier)
{
     __block NSFileProviderManager *manager = nil;
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

        BOOL foundDomain = NO;

        for (NSFileProviderDomain *const domain in domains) {
            if ([domain.identifier isEqualToString:nsDomainIdentifier]) {
                 foundDomain = YES;
                 manager = [NSFileProviderManager managerForDomain:domain];
            }
        }

        if (!foundDomain) {
            qCWarning(lcMacFileProviderUtils) << "No matching item domain, cannot get manager";
        }

        dispatch_semaphore_signal(semaphore);
    }];

    dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

    if (manager == nil) {
        qCWarning(lcMacFileProviderUtils) << "Received null manager for domain"
                                          << domainIdentifier;
    }

    return manager;
}

} // namespace FileProviderUtils

} // namespace Mac

} // namespace OCC
