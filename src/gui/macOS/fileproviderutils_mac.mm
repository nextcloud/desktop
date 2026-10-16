/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "configfile.h"
#include "fileproviderutils.h"
#include "fileproviderutils_mac.h"
#include "account.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QString>

#import <FileProvider/FileProvider.h>

namespace
{
constexpr auto fileProviderRequestTimeoutMs = 5000;
}

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

Q_LOGGING_CATEGORY(lcMacFileProviderUtils, "nextcloud.gui.macfileproviderutils", QtInfoMsg)

bool waitForFileProviderReplies(const NSUInteger requestCount, const char *operation, void (^startRequests)(RequestReplyHandler), NSArray **results)
{
    if (requestCount == 0) {
        *results = @[];
        return true;
    }

    const auto values = [NSMutableArray new];
    const auto semaphore = dispatch_semaphore_create(0);
    __block auto remaining = requestCount;
    __block auto closed = false;
    __block NSError *replyError = nil;

    startRequests(^BOOL(id value, NSError *error) {
        @synchronized(values) {
            if (closed) {
                return NO;
            }

            if (error != nil) {
                if (replyError == nil) {
                    replyError = error;
                }
            } else if (value != nil) {
                [values addObject:value];
            }
            if (--remaining == 0) {
                closed = true;
                dispatch_semaphore_signal(semaphore);
            }
            return YES;
        }
    });

    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, fileProviderRequestTimeoutMs * NSEC_PER_MSEC));
    auto completed = false;
    NSError *acceptedError = nil;
    @synchronized(values) {
        closed = true;
        completed = remaining == 0;
        *results = values.copy;
        acceptedError = replyError;
    }

    if (!completed) {
        qCWarning(lcMacFileProviderUtils) << "Timed out while" << operation;
        return false;
    }
    if (acceptedError != nil) {
        qCWarning(lcMacFileProviderUtils) << "Error while" << operation << acceptedError.code << acceptedError.localizedDescription;
        return false;
    }

    return true;
}

bool waitForFileProviderRequest(const char *operation, void (^startRequest)(RequestReplyHandler), id *result)
{
    if (result != nullptr) {
        *result = nil;
    }
    NSArray *values = nil;
    const auto success = waitForFileProviderReplies(1, operation, startRequest, &values);
    if (success && result != nullptr) {
        *result = values.firstObject;
    }
    return success;
}

NSArray<NSFileProviderDomain *> *fileProviderDomains(bool *success)
{
    id result = nil;
    const auto received = waitForFileProviderRequest(
        "getting file provider domains",
        ^(RequestReplyHandler reply) {
            [NSFileProviderManager
                getDomainsWithCompletionHandler:^(NSArray<NSFileProviderDomain *> *domains, NSError *error) { reply([domains copy], error); }];
        },
        &result);
    if (success != nullptr) {
        *success = received;
    }
    return result;
}
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
    const auto domains = fileProviderDomains();
    if (domains == nil) {
        return nil;
    }

    NSFileProviderDomain *domain = nil;
    for (NSFileProviderDomain * const candidate in domains) {
        if (domainIdentifier == QString::fromNSString(candidate.identifier)) {
            domain = candidate;
            break;
        }
    }

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
