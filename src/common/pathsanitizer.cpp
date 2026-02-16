/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "pathsanitizer.h"

#include <QDir>
#include <QLoggingCategory>
#include <QRegularExpression>

namespace OCC {

Q_LOGGING_CATEGORY(lcPathSanitizer, "nextcloud.common.pathsanitizer", QtInfoMsg)

QString PathSanitizer::forbiddenCharacters()
{
    return QStringLiteral("\\:*?\"<>|");
}

bool PathSanitizer::containsForbiddenCharacters(const QString &name)
{
    const auto forbidden = forbiddenCharacters();
    for (const auto &ch : name) {
        if (forbidden.contains(ch)) {
            return true;
        }
    }
    return false;
}

bool PathSanitizer::containsPathTraversal(const QString &path)
{
    const auto components = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const auto &component : components) {
        if (component == QLatin1String("..")) {
            return true;
        }
    }
    return false;
}

QString PathSanitizer::removeConsecutiveSeparators(const QString &path)
{
    if (path.isEmpty()) {
        return path;
    }

    QString result;
    result.reserve(path.size());

    bool lastWasSeparator = false;
    for (const auto &ch : path) {
        const bool isSeparator = (ch == QLatin1Char('/') || ch == QLatin1Char('\\'));
        if (isSeparator) {
            if (!lastWasSeparator) {
                result.append(QLatin1Char('/'));
            }
            lastWasSeparator = true;
        } else {
            result.append(ch);
            lastWasSeparator = false;
        }
    }

    return result;
}

bool PathSanitizer::hasTrailingDotOrSpace(const QString &component)
{
    if (component.isEmpty()) {
        return false;
    }
    const auto lastChar = component.at(component.size() - 1);
    return lastChar == QLatin1Char('.') || lastChar == QLatin1Char(' ');
}

bool PathSanitizer::exceedsMaxComponentLength(const QString &component, int maxLength)
{
    return component.length() > maxLength;
}

QString PathSanitizer::sanitizeComponent(const QString &component)
{
    if (component.isEmpty()) {
        return component;
    }

    QString result;
    result.reserve(component.size());

    const auto forbidden = forbiddenCharacters();
    for (const auto &ch : component) {
        if (forbidden.contains(ch)) {
            result.append(QLatin1Char('_'));
        } else if (ch.unicode() < 32) {
            // Skip control characters
            continue;
        } else {
            result.append(ch);
        }
    }

    // Remove trailing dots and spaces (problematic on Windows)
    while (!result.isEmpty()) {
        const auto lastChar = result.at(result.size() - 1);
        if (lastChar == QLatin1Char('.') || lastChar == QLatin1Char(' ')) {
            result.chop(1);
        } else {
            break;
        }
    }

    // Truncate to 255 characters if necessary
    if (result.length() > 255) {
        result.truncate(255);
    }

    return result;
}

PathSanitizer::ValidationResult PathSanitizer::validatePath(const QString &path)
{
    ValidationResult result;

    if (path.isEmpty()) {
        result.isValid = false;
        result.warnings.append(QStringLiteral("Path is empty"));
        return result;
    }

    if (containsPathTraversal(path)) {
        result.isValid = false;
        result.warnings.append(QStringLiteral("Path contains directory traversal sequence (..)"));
        return result;
    }

    // Normalize separators
    QString normalizedPath = removeConsecutiveSeparators(path);
    if (normalizedPath != path) {
        result.warnings.append(QStringLiteral("Path contained consecutive separators"));
    }

    // Check and sanitize each component
    const auto components = normalizedPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList sanitizedComponents;
    sanitizedComponents.reserve(components.size());

    for (const auto &component : components) {
        if (containsForbiddenCharacters(component)) {
            result.warnings.append(QStringLiteral("Component \"%1\" contains forbidden characters").arg(component));
        }

        if (hasTrailingDotOrSpace(component)) {
            result.warnings.append(QStringLiteral("Component \"%1\" has trailing dot or space").arg(component));
        }

        if (exceedsMaxComponentLength(component)) {
            result.warnings.append(QStringLiteral("Component \"%1\" exceeds maximum length of 255 characters").arg(component));
        }

        sanitizedComponents.append(sanitizeComponent(component));
    }

    // Reconstruct the path
    const bool hadLeadingSeparator = normalizedPath.startsWith(QLatin1Char('/'));
    result.sanitizedPath = sanitizedComponents.join(QLatin1Char('/'));
    if (hadLeadingSeparator) {
        result.sanitizedPath.prepend(QLatin1Char('/'));
    }

    result.isValid = result.warnings.isEmpty();
    return result;
}

} // namespace OCC
