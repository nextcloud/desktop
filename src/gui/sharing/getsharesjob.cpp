/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getsharesjob.h"

#include "share.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
QList<QPair<QString, QString>> getSharesParameters(const std::optional<QString> &sourceTypeClass,
                                                   const std::optional<QString> &sourceTypeValue,
                                                   const std::optional<QString> &lastShareId,
                                                   qint64 limit)
{
    auto parameters = QList<QPair<QString, QString>>{{"limit"_L1, QString::number(limit)}};
    if (sourceTypeClass) {
        parameters.emplaceBack("filterSourceTypeClass"_L1, *sourceTypeClass);
    }
    if (sourceTypeValue) {
        parameters.emplaceBack("filterSourceTypeValue"_L1, *sourceTypeValue);
    }
    if (lastShareId) {
        parameters.emplaceBack("lastShareID"_L1, *lastShareId);
    }
    return parameters;
}
}

GetSharesJob::GetSharesJob(AccountPtr account,
                           const std::optional<QString> &sourceTypeClass,
                           const std::optional<QString> &sourceTypeValue,
                           const std::optional<QString> &lastShareId,
                           qint64 limit)
    : UnifiedSharingRequest{account,
                            "/ocs/v2.php/apps/sharing/api/v1/shares"_L1,
                            "GET"_ba,
                            {.parameters = getSharesParameters(sourceTypeClass, sourceTypeValue, lastShareId, limit)}}
{
    connect(this, &OcsJob::jobFinished, this, [this, account = std::move(account)](const QJsonDocument &json, int) {
        auto shares = QList<QPointer<Share>>{};
        const auto data = json.object().value("ocs"_L1).toObject().value("data"_L1).toArray();
        shares.reserve(data.size());
        for (const auto &value : data) {
            const auto shareJson = QJsonDocument{QJsonObject{
                {"ocs"_L1, QJsonObject{{"data"_L1, value.toObject()}}},
            }};
            shares.append(Share::fromJson(shareJson, account));
        }
        Q_EMIT sharesFetched(shares);
    });
}

}
