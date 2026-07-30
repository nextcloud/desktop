/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "macsandboxpersistentaccess.h"

#include <QLoggingCategory>
#include <QString>

#import <Foundation/Foundation.h>

Q_LOGGING_CATEGORY(lcMacSandboxPersistent, "nextcloud.common.mac.sandbox.persistent", QtInfoMsg)

namespace OCC {
namespace Utility {

class MacSandboxPersistentAccess::Impl
{
public:
    explicit Impl(const QByteArray &bookmarkData)
        : _nsUrl(nullptr)
        , _hasAccess(false)
        , _isStale(false)
    {
        if (bookmarkData.isEmpty()) {
            qCWarning(lcMacSandboxPersistent) << "Empty bookmark data provided";
            return;
        }

        @autoreleasepool {
            NSData *nsBookmarkData = [NSData dataWithBytes:bookmarkData.constData()
                                                    length:bookmarkData.size()];
            if (!nsBookmarkData) {
                qCWarning(lcMacSandboxPersistent) << "Failed to create NSData from bookmark data";
                return;
            }

            BOOL isStale = NO;
            NSError *error = nil;
            _nsUrl = [[NSURL URLByResolvingBookmarkData:nsBookmarkData
                                                options:NSURLBookmarkResolutionWithSecurityScope
                                          relativeToURL:nil
                                    bookmarkDataIsStale:&isStale
                                                  error:&error] retain];

            if (error) {
                qCWarning(lcMacSandboxPersistent) << "Failed to resolve bookmark data:"
                                        << QString::fromNSString([error localizedDescription]);
                return;
            }

            if (!_nsUrl) {
                qCWarning(lcMacSandboxPersistent) << "Resolved URL is nil";
                return;
            }

            if (isStale) {
                _isStale = true;
                qCWarning(lcMacSandboxPersistent) << "Bookmark data is stale for path:"
                                        << QString::fromNSString([_nsUrl path])
                                        << "- the bookmark should be recreated";
            }

            _hasAccess = [_nsUrl startAccessingSecurityScopedResource];

            if (_hasAccess) {
                qCInfo(lcMacSandboxPersistent) << "Successfully started persistent access to security-scoped resource:"
                                     << QString::fromNSString([_nsUrl path]);
            } else {
                qCWarning(lcMacSandboxPersistent) << "Failed to start persistent access to security-scoped resource:"
                                        << QString::fromNSString([_nsUrl path]);
            }
        }
    }

    ~Impl()
    {
        @autoreleasepool {
            if (_hasAccess && _nsUrl) {
                [_nsUrl stopAccessingSecurityScopedResource];
                qCDebug(lcMacSandboxPersistent) << "Stopped persistent access to security-scoped resource";
                _hasAccess = false;
            }

            if (_nsUrl) {
                [_nsUrl release];
                _nsUrl = nullptr;
            }
        }
    }

    // Non-copyable
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;

    [[nodiscard]] bool hasAccess() const { return _hasAccess; }
    [[nodiscard]] bool isStale() const { return _isStale; }

private:
    NSURL *_nsUrl;
    bool _hasAccess;
    bool _isStale;
};

MacSandboxPersistentAccess::MacSandboxPersistentAccess(const QByteArray &bookmarkData)
    : _impl(std::make_unique<Impl>(bookmarkData))
{
}

MacSandboxPersistentAccess::~MacSandboxPersistentAccess() = default;

MacSandboxPersistentAccess::MacSandboxPersistentAccess(MacSandboxPersistentAccess &&) noexcept = default;

MacSandboxPersistentAccess &MacSandboxPersistentAccess::operator=(MacSandboxPersistentAccess &&) noexcept = default;

std::unique_ptr<MacSandboxPersistentAccess> MacSandboxPersistentAccess::createFromBookmarkData(const QByteArray &bookmarkData)
{
    if (bookmarkData.isEmpty()) {
        return nullptr;
    }
    return std::unique_ptr<MacSandboxPersistentAccess>(new MacSandboxPersistentAccess(bookmarkData));
}

bool MacSandboxPersistentAccess::isValid() const
{
    return _impl && _impl->hasAccess();
}

bool MacSandboxPersistentAccess::isStale() const
{
    return _impl && _impl->isStale();
}

} // namespace Utility
} // namespace OCC
