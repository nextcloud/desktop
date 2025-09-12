/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"
#include "fileproviderutils.h"
#include "account.h"

#include <QLoggingCategory>
#include <QRegularExpression>
#include <QString>

#import <FileProvider/FileProvider.h>

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

Q_LOGGING_CATEGORY(lcMacFileProviderUtils, "nextcloud.gui.macfileproviderutils", QtInfoMsg)

inline bool hasBundleExtension(const QString &domainId)
{
    return std::any_of(bundleExtensions.begin(), bundleExtensions.end(), [&domainId](const auto &ext) {
        return domainId.endsWith(ext);
    });
}

bool illegalDomainIdentifier(const QString &domainId)
{
    return !domainId.isEmpty() && !illegalChars.match(domainId).hasMatch() && hasBundleExtension(domainId);
}

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
            qCWarning(lcMacFileProviderUtils) << "Error fetching file provider domains:"
                                              << error.localizedDescription;
            dispatch_semaphore_signal(semaphore);
            return;
        }

        for (NSFileProviderDomain *const domain in domains) {
            qCInfo(lcMacFileProviderUtils) << "Found file provider domain with identifier:"
                                           << domain.identifier;

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
        qCWarning(lcMacFileProviderUtils) << "No matching file provider domain found for identifier: "
                                          << domainIdentifier;
    }

    return foundDomain;
}

QString domainIdentifierForAccountIdentifier(const QString &accountId)
{
    qCDebug(lcMacFileProviderUtils) << "Resolving file provider domain identifier by account id:"
                                         << accountId;

    OCC::ConfigFile cfg;
    const QString existingUuid = cfg.fileProviderDomainUuidFromAccountId(accountId);

    if (!existingUuid.isEmpty()) {
        qCDebug(lcMacFileProviderUtils) << "Found existing file provider domain UUID to return:"
                                                     << existingUuid
                                                     << "for account id:"
                                                     << accountId;

        return existingUuid;
    }

    auto domainId = accountId;
    Q_ASSERT(!domainId.isEmpty());

    domainId.replace(illegalChars, "-");

    // Some url domains like .app cause issues on macOS as these are also bundle extensions.
    // Under the hood, fileproviderd will create a folder for the user to access the files named
    // after the domain identifier. If the url domain is the same as a bundle extension, Finder
    // will interpret this folder as a bundle and will not allow the user to access the files.
    // Here we wrap the dot in the url domain extension to prevent this from happening.
    for (const auto &ext : bundleExtensions) {
        if (domainId.endsWith(ext)) {
            domainId = domainId.left(domainId.length() - ext.length());
            domainId += "(.)" + ext.right(ext.length() - 1);
            break;
        }
    }

    return domainId;
}

QString domainIdentifierForAccountIdentifier(const NSString *accountId)
{
    const auto qAccountId = QString::fromNSString(accountId);

    return domainIdentifierForAccountIdentifier(qAccountId);
}

QString domainIdentifierForAccount(const OCC::Account * const account)
{
    Q_ASSERT(account);
    auto accountId = account->userIdAtHostWithPort();

    return domainIdentifierForAccountIdentifier(accountId);
}

QString domainIdentifierForAccount(const OCC::AccountPtr account)
{
    return domainIdentifierForAccount(account.get());
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
