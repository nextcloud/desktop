/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "ocsynclib.h"

#include <QString>
#include <QStringList>

namespace OCC {

/**
 * @brief Utility class for sanitizing and validating file paths for sync operations.
 *
 * Handles common path issues such as trailing whitespace, consecutive separators,
 * forbidden characters, and path traversal attempts. This class helps prevent
 * sync errors caused by invalid or problematic file paths on various platforms.
 *
 * @ingroup libsync
 */
class OCSYNC_EXPORT PathSanitizer
{
public:
    /**
     * @brief Result of a path validation check.
     */
    struct ValidationResult {
        bool isValid = false;
        QString sanitizedPath;
        QStringList warnings;
    };

    /**
     * @brief Validates and optionally sanitizes a file path for sync operations.
     *
     * Checks for:
     * - Empty paths
     * - Path traversal attempts (e.g., "../")
     * - Forbidden characters on Windows (: * ? " < > |)
     * - Trailing dots and spaces in path components (problematic on Windows)
     * - Consecutive path separators
     * - Excessively long path components (> 255 characters)
     *
     * @param path The path to validate (sync-folder relative)
     * @return ValidationResult containing the sanitized path and any warnings
     */
    [[nodiscard]] static ValidationResult validatePath(const QString &path);

    /**
     * @brief Sanitizes a single path component (filename or directory name).
     *
     * Removes or replaces characters that are invalid on common file systems.
     *
     * @param component A single path component (no separators)
     * @return The sanitized component
     */
    [[nodiscard]] static QString sanitizeComponent(const QString &component);

    /**
     * @brief Checks if a path contains directory traversal sequences.
     *
     * Detects ".." components that could be used to escape the sync root.
     *
     * @param path The path to check
     * @return true if path traversal is detected
     */
    [[nodiscard]] static bool containsPathTraversal(const QString &path);

    /**
     * @brief Removes consecutive path separators from a path.
     *
     * Normalizes "foo//bar///baz" to "foo/bar/baz".
     *
     * @param path The path to normalize
     * @return The normalized path
     */
    [[nodiscard]] static QString removeConsecutiveSeparators(const QString &path);

    /**
     * @brief Checks if a filename contains characters forbidden on Windows.
     *
     * The forbidden characters are: \\ : * ? " < > |
     *
     * @param name The filename to check
     * @return true if forbidden characters are found
     */
    [[nodiscard]] static bool containsForbiddenCharacters(const QString &name);

    /**
     * @brief Returns the list of characters forbidden in filenames on Windows.
     *
     * @return A string containing all forbidden characters
     */
    [[nodiscard]] static QString forbiddenCharacters();

    /**
     * @brief Checks if a path component ends with a dot or space.
     *
     * Such components are problematic on Windows where trailing dots and
     * spaces are silently removed by the OS.
     *
     * @param component The path component to check
     * @return true if the component has a trailing dot or space
     */
    [[nodiscard]] static bool hasTrailingDotOrSpace(const QString &component);

    /**
     * @brief Checks if a path component exceeds the maximum allowed length.
     *
     * Most file systems have a 255-character limit for individual path components.
     *
     * @param component The path component to check
     * @param maxLength The maximum allowed length (default: 255)
     * @return true if the component exceeds the maximum length
     */
    [[nodiscard]] static bool exceedsMaxComponentLength(const QString &component, int maxLength = 255);

private:
    PathSanitizer() = default;
};

} // namespace OCC
