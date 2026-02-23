/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QByteArray>
#include <memory>

namespace OCC {
namespace Utility {

/**
 * @brief RAII wrapper for persistent security-scoped resource access
 *
 * Unlike MacSandboxSecurityScopedAccess which is intended for short-lived
 * access during a single operation, this class manages long-lived access
 * to a security-scoped URL resolved from persisted bookmark data.
 *
 * Designed to be stored on Folder objects for the folder's entire lifetime,
 * ensuring the sandboxed app retains access to sync folder paths across
 * restart boundaries.
 *
 * Usage:
 * @code
 * QByteArray bookmarkData = folderDefinition.securityScopedBookmarkData;
 * auto access = MacSandboxPersistentAccess::createFromBookmarkData(bookmarkData);
 * if (access && access->isValid()) {
 *     // The local sync folder path is now accessible
 * }
 * @endcode
 */
class MacSandboxPersistentAccess
{
public:
    ~MacSandboxPersistentAccess();

    // Non-copyable, movable
    MacSandboxPersistentAccess(const MacSandboxPersistentAccess &) = delete;
    MacSandboxPersistentAccess &operator=(const MacSandboxPersistentAccess &) = delete;
    MacSandboxPersistentAccess(MacSandboxPersistentAccess &&) noexcept;
    MacSandboxPersistentAccess &operator=(MacSandboxPersistentAccess &&) noexcept;

    /**
     * @brief Create a persistent access wrapper by resolving bookmark data
     * @param bookmarkData The app-scoped bookmark data previously created via createSecurityScopedBookmarkData()
     * @return A unique pointer to the access wrapper, or nullptr if bookmarkData is empty
     */
    [[nodiscard]] static std::unique_ptr<MacSandboxPersistentAccess> createFromBookmarkData(const QByteArray &bookmarkData);

    /**
     * @brief Check if the security-scoped access was successfully acquired
     * @return true if access is valid and the path can be accessed
     */
    [[nodiscard]] bool isValid() const;

    /**
     * @brief Check if the resolved bookmark was reported as stale by macOS
     *
     * A stale bookmark still works, but may stop working in the future.
     * When stale, callers should recreate the bookmark data via
     * createSecurityScopedBookmarkData() and persist the new data.
     *
     * @return true if the bookmark was stale when resolved
     */
    [[nodiscard]] bool isStale() const;

private:
    explicit MacSandboxPersistentAccess(const QByteArray &bookmarkData);

    class Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace Utility
} // namespace OCC
