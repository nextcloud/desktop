/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "utility_mac_sandbox.h"

#include <QLoggingCategory>

#import <Foundation/Foundation.h>

Q_LOGGING_CATEGORY(lcMacSandboxUtility, "nextcloud.common.mac.sandbox.utility", QtInfoMsg)

namespace OCC {
namespace Utility {

// --- Free functions ---

QByteArray createSecurityScopedBookmarkData(const QString &localPath)
{
    if (localPath.isEmpty()) {
        qCWarning(lcMacSandboxUtility) << "Empty path provided for bookmark creation";
        return {};
    }

    @autoreleasepool {
        NSURL *url = [NSURL fileURLWithPath:localPath.toNSString()];
        if (!url) {
            qCWarning(lcMacSandboxUtility) << "Failed to create NSURL from path:" << localPath;
            return {};
        }

        NSError *error = nil;
        NSData *bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
                             includingResourceValuesForKeys:nil
                                              relativeToURL:nil
                                                      error:&error];

        if (error || !bookmarkData) {
            qCWarning(lcMacSandboxUtility) << "Failed to create bookmark data for path:" << localPath
                                    << (error ? QString::fromNSString([error localizedDescription]) : QStringLiteral("(unknown error)"));
            return {};
        }

        QByteArray result(reinterpret_cast<const char *>([bookmarkData bytes]),
                          static_cast<int>([bookmarkData length]));

        qCInfo(lcMacSandboxUtility) << "Successfully created security-scoped bookmark data for:" << localPath
                             << "(" << result.size() << "bytes)";
        return result;
    }
}

QString getRealHomeDirectory()
{
    @autoreleasepool {
        NSString *homeDir = NSHomeDirectory();
        return QString::fromNSString(homeDir);
    }
}

} // namespace Utility
} // namespace OCC
