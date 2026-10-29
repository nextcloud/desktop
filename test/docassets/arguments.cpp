/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "arguments.h"

namespace OCC::DocAssets
{
QStringList normalizedCaptureArguments(const QStringList &arguments, QString *error)
{
    error->clear();
    if (arguments.isEmpty()) {
        *error = QStringLiteral("Missing executable name");
        return {};
    }
    QStringList result{arguments.first()};
    for (auto index = 1; index < arguments.size(); ++index) {
        const auto &argument = arguments.at(index);
        if (argument == QStringLiteral("-ApplePersistenceIgnoreState") || argument == QStringLiteral("-NSDocumentRevisionsDebugMode")
            || argument == QStringLiteral("--NSDocumentRevisionsDebugMode")) {
            if (index + 1 >= arguments.size()) {
                *error = QStringLiteral("Missing boolean value for %1").arg(argument);
                return {};
            }
            const auto value = arguments.at(index + 1).toLower();
            if (value != QStringLiteral("yes") && value != QStringLiteral("no") && value != QStringLiteral("true") && value != QStringLiteral("false")
                && value != QStringLiteral("1") && value != QStringLiteral("0")) {
                *error = QStringLiteral("Invalid boolean value for %1").arg(argument);
                return {};
            }
            ++index;
            continue;
        }
        result.append(argument);
        // Keep capture operands intact, even if an invalid operand resembles an option.
        if (argument == QStringLiteral("--output") && index + 1 < arguments.size()) {
            result.append(arguments.at(++index));
        }
    }
    return result;
}
}
