/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getsharesjob.h"

#include <QJsonDocument>

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
                            {.parameters = getSharesParameters(sourceTypeClass, sourceTypeValue, lastShareId, limit), .passStatusCodes = {}, .body = {}}}
{
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT sharesFetched(json);
    });
}

}
