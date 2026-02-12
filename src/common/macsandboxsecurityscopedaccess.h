/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QUrl>
#include <memory>

namespace OCC {
namespace Utility {

/**
 * @brief RAII wrapper for macOS security-scoped resource access
 * 
 * When working with files selected by the user via QFileDialog in a sandboxed
 * macOS app, the returned URLs are security-scoped bookmarks that require
 * explicit access management via startAccessingSecurityScopedResource() and
 * stopAccessingSecurityScopedResource().
 * 
 * This class provides RAII semantics to ensure proper cleanup.
 * 
 * Usage:
 * @code
 * QUrl fileUrl = QFileDialog::getSaveFileUrl(...);
 * if (!fileUrl.isEmpty()) {
 *     auto scopedAccess = MacSandboxSecurityScopedAccess::create(fileUrl);
 *     if (scopedAccess->isValid()) {
 *         // Now you can access the file
 *         QFile file(fileUrl.toLocalFile());
 *         file.open(QIODevice::WriteOnly);
 *     }
 * }
 * @endcode
 */
class MacSandboxSecurityScopedAccess
{
public:
    ~MacSandboxSecurityScopedAccess();

    // Non-copyable, movable
    MacSandboxSecurityScopedAccess(const MacSandboxSecurityScopedAccess&) = delete;
    MacSandboxSecurityScopedAccess& operator=(const MacSandboxSecurityScopedAccess&) = delete;
    MacSandboxSecurityScopedAccess(MacSandboxSecurityScopedAccess&&) noexcept;
    MacSandboxSecurityScopedAccess& operator=(MacSandboxSecurityScopedAccess&&) noexcept;

    /**
     * @brief Create a security-scoped access wrapper for the given URL
     * @param url The URL to access (typically from QFileDialog)
     * @return A unique pointer to the access wrapper
     */
    [[nodiscard]] static std::unique_ptr<MacSandboxSecurityScopedAccess> create(const QUrl &url);

    /**
     * @brief Check if the security-scoped access was successfully acquired
     * @return true if access is valid and the file can be accessed
     */
    [[nodiscard]] bool isValid() const;

private:
    explicit MacSandboxSecurityScopedAccess(const QUrl &url);

    class Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace Utility
} // namespace OCC
