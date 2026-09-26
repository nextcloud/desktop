/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "sharingsource.h"
#include "sharing/sharingconstants.h"
#include <QJsonArray>
#include <QJsonObject>

namespace OCC::DocAssets
{
QVariantMap sharingSourceProperties(const QJsonDocument &response, QString *error)
{
    error->clear();
    const auto data = response.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data"));
    if (!data.isArray()) {
        *error = QStringLiteral("The server returned an invalid sharing list.");
        return {};
    }
    for (const auto &entry : data.toArray()) {
        const auto share = entry.toObject();
        if (share.value(QStringLiteral("id")).toString().isEmpty()) {
            continue;
        }
        for (const auto &value : share.value(QStringLiteral("sources")).toArray()) {
            const auto source = value.toObject();
            const auto fileId = source.value(QStringLiteral("value")).toString();
            const auto name = source.value(QStringLiteral("display_name")).toString();
            if (source.value(QStringLiteral("class")).toString() == Gui::Sharing::SourceTypeClasses::node && !fileId.isEmpty() && !name.isEmpty()) {
                return {{QStringLiteral("fileId"), fileId}, {QStringLiteral("shortLocalPath"), name}};
            }
        }
    }
    *error = QStringLiteral("No existing file share was found in the first 100 server shares. Create a file share on the test account and retry.");
    return {};
}
}
