/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "utility_mac_sandbox.h"

#include <QFile>
#include <QLoggingCategory>

#include <pwd.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

static_assert(__has_feature(objc_arc), "The macOS sandbox utilities require ARC.");

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
    // In the sandbox NSHomeDirectory(), NSHomeDirectoryForUser() and $HOME all point into the app container, the user database does not.
    const auto *const userEntry = getpwuid(getuid());
    if (!userEntry || !userEntry->pw_dir) {
        qCWarning(lcMacSandboxUtility) << "Failed to resolve the logged-in user's home directory";
        return {};
    }
    return QFile::decodeName(userEntry->pw_dir);
}

} // namespace Utility
} // namespace OCC
