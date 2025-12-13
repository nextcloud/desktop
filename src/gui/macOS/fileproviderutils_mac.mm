/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"
#include "fileproviderutils.h"
#include "account.h"

#include <QCoreApplication>
#include <QDir>
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

QString applicationGroupContainer()
{
    NSString *const groupId = (NSString *)[NSBundle.mainBundle objectForInfoDictionaryKey:@"NCFPKAppGroupIdentifier"];

    if (groupId == nil) {
        qCWarning(lcMacFileProviderUtils) << "No app group identifier found in Info.plist, cannot determine application group container.";
        return QString::fromNSString([NSFileManager.defaultManager temporaryDirectory].path);
    }

    return QString::fromNSString([NSFileManager.defaultManager containerURLForSecurityApplicationGroupIdentifier:groupId].path);
}

QDir fileProviderDomainLogDirectory(const QString domainIdentifier)
{
    auto logsDirectory = fileProviderDomainSupportDirectory(domainIdentifier);
    auto domainLogsPath = logsDirectory.filePath("Logs");
    auto directory = QDir(domainLogsPath);

    return directory;
}

QDir fileProviderDomainsSupportDirectory()
{
    auto applicationGroupContainerPath = applicationGroupContainer();
    auto supportPath = applicationGroupContainerPath + "/File Provider Domains";
    auto directory = QDir(supportPath);

    return directory;
}

QDir fileProviderDomainSupportDirectory(const QString domainIdentifier)
{
    auto supportDirectory = fileProviderDomainsSupportDirectory();
    auto domainSupportPath = supportDirectory.filePath(domainIdentifier);
    auto directory = QDir(domainSupportPath);

    return directory;
}

NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier)
{
    dispatch_group_t dispatchGroup = dispatch_group_create();
    dispatch_group_enter(dispatchGroup);

    __block NSFileProviderDomain *domain = nil;

    [NSFileProviderManager getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> * const domains, NSError * const error) {
        if (error != nil) {
            qCWarning(lcMacFileProviderUtils) << "Could not get existing domains because of error:"
                                              << error.code
                                              << error.localizedDescription;
            dispatch_group_leave(dispatchGroup);
            return;
        }

        for (NSFileProviderDomain * const candidate in domains) {
            if (domainIdentifier == QString::fromNSString(candidate.identifier)) {
                domain = [candidate retain];
                break;
            }
        }

        dispatch_group_leave(dispatchGroup);
    }];

    dispatch_group_wait(dispatchGroup, DISPATCH_TIME_FOREVER);

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
