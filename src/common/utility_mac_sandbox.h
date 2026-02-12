/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "macsandboxsecurityscopedaccess.h"
#include "macsandboxpersistentaccess.h"

#include <QByteArray>
#include <QString>

namespace OCC {
namespace Utility {

/**
 * @brief Create app-scoped security-scoped bookmark data for a local path
 *
 * This should be called while the app still has access to the path (e.g. right
 * after the user selects a folder via QFileDialog). The returned data can be
 * persisted to settings and later resolved with
 * MacSandboxPersistentAccess::createFromBookmarkData() to regain access
 * after an app restart.
 *
 * @param localPath The local file system path to create a bookmark for
 * @return The bookmark data as a QByteArray, or an empty QByteArray on failure
 */
[[nodiscard]] QByteArray createSecurityScopedBookmarkData(const QString &localPath);

/**
 * @brief Get the real user home directory path
 *
 * In sandboxed macOS apps, QStandardPaths::HomeLocation returns the sandbox
 * container directory, not the actual user home directory. This function uses
 * NSHomeDirectory() to retrieve the real home directory path.
 *
 * @return The real user home directory path
 */
[[nodiscard]] QString getRealHomeDirectory();

} // namespace Utility
} // namespace OCC
