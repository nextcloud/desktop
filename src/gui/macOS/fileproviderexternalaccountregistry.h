/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace OCC::Mac::FileProviderExternalAccountRegistry
{

[[nodiscard]] inline QStringList configuredAccountIdentifiers(const QList<QPair<QString, QString>> &accountStorage)
{
    auto identifiers = QStringList{};
    for (const auto &[identifier, volumeUuid] : accountStorage) {
        if (!volumeUuid.isEmpty()) {
            identifiers.append(identifier);
        }
    }

    identifiers.removeDuplicates();
    identifiers.sort(Qt::CaseSensitive);
    return identifiers;
}

} // namespace OCC::Mac::FileProviderExternalAccountRegistry
