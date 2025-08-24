/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileproviderutils.h"

#include <QLoggingCategory>
#include <QRegularExpression>
#include <QString>

#import <FileProvider/FileProvider.h>

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

Q_LOGGING_CATEGORY(lcMacFileProviderUtils, "nextcloud.gui.macfileproviderutils", QtInfoMsg)

/**
 * This list is not exhaustive already because third-party apps can define their own proprietary bundle types arbitrarily.
 * This list only coveers the most common extensions.
 */
static constexpr auto bundleExtensions = std::array{
    QLatin1StringView(".app"),
    QLatin1StringView(".framework"),
    QLatin1StringView(".kext"),
    QLatin1StringView(".plugin"),
    QLatin1StringView(".docset"),
    QLatin1StringView(".xpc"),
    QLatin1StringView(".qlgenerator"),
    QLatin1StringView(".component"),
    QLatin1StringView(".saver"),
    QLatin1StringView(".mdimporter")
};

static const QRegularExpression illegalChars("[:/]");

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

QString domainIdentifierForUserIdAtHostWithPort(const QString userIdAtHostWithPort)
{
    auto domainId = userIdAtHostWithPort;
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
