/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macsandboxsecurityscopedaccess.h"

#include <QLoggingCategory>

#import <Foundation/Foundation.h>

Q_LOGGING_CATEGORY(lcMacSandbox, "nextcloud.common.mac.sandbox", QtInfoMsg)

namespace OCC {
namespace Utility {

class MacSandboxSecurityScopedAccess::Impl
{
public:
    explicit Impl(const QUrl &url)
        : _url(url)
        , _nsUrl(nullptr)
        , _hasAccess(false)
    {
        if (!url.isValid() || url.isEmpty()) {
            qCWarning(lcMacSandbox) << "Invalid or empty URL provided";
            return;
        }

        // Convert QUrl to NSURL
        const QString localPath = url.toLocalFile();
        if (localPath.isEmpty()) {
            qCWarning(lcMacSandbox) << "URL does not represent a local file:" << url;
            return;
        }

        @autoreleasepool {
            _nsUrl = [[NSURL fileURLWithPath:localPath.toNSString()] retain];
            
            if (!_nsUrl) {
                qCWarning(lcMacSandbox) << "Failed to create NSURL from path:" << localPath;
                return;
            }

            // Start accessing the security-scoped resource
            _hasAccess = [_nsUrl startAccessingSecurityScopedResource];
            
            if (_hasAccess) {
                qCDebug(lcMacSandbox) << "Successfully started accessing security-scoped resource:" << localPath;
            } else {
                qCWarning(lcMacSandbox) << "Failed to start accessing security-scoped resource:" << localPath;
            }
        }
    }

    ~Impl()
    {
        @autoreleasepool {
            if (_hasAccess && _nsUrl) {
                [_nsUrl stopAccessingSecurityScopedResource];
                qCDebug(lcMacSandbox) << "Stopped accessing security-scoped resource";
                _hasAccess = false;
            }
            
            if (_nsUrl) {
                [_nsUrl release];
                _nsUrl = nullptr;
            }
        }
    }

    // Non-copyable
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    [[nodiscard]] bool hasAccess() const { return _hasAccess; }

private:
    QUrl _url;
    NSURL *_nsUrl;
    bool _hasAccess;
};

MacSandboxSecurityScopedAccess::MacSandboxSecurityScopedAccess(const QUrl &url)
    : _impl(std::make_unique<Impl>(url))
{
}

MacSandboxSecurityScopedAccess::~MacSandboxSecurityScopedAccess() = default;

MacSandboxSecurityScopedAccess::MacSandboxSecurityScopedAccess(MacSandboxSecurityScopedAccess&&) noexcept = default;

MacSandboxSecurityScopedAccess& MacSandboxSecurityScopedAccess::operator=(MacSandboxSecurityScopedAccess&&) noexcept = default;

std::unique_ptr<MacSandboxSecurityScopedAccess> MacSandboxSecurityScopedAccess::create(const QUrl &url)
{
    return std::unique_ptr<MacSandboxSecurityScopedAccess>(new MacSandboxSecurityScopedAccess(url));
}

bool MacSandboxSecurityScopedAccess::isValid() const
{
    return _impl && _impl->hasAccess();
}

} // namespace Utility
} // namespace OCC
