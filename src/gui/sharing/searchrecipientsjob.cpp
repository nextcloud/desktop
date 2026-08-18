/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchrecipientsjob.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
constexpr auto requestTimeoutMsec = 10 * 1000;

QList<QPair<QString, QString>> searchRecipientsParameters(const QString &query,
                                                          qint64 offset,
                                                          qint64 limit,
                                                          const QList<QString> &recipientTypeClasses,
                                                          const std::optional<QString> &shareId)
{
    auto parameters = QList<QPair<QString, QString>>{
        {"query"_L1, query},
        {"offset"_L1, QString::number(offset)},
        {"limit"_L1, QString::number(limit)},
    };
    for (const auto &recipientTypeClass : recipientTypeClasses) {
        parameters.emplaceBack("filterRecipientTypeClasses[]"_L1, recipientTypeClass);
    }
    if (shareId) {
        parameters.emplaceBack("id"_L1, *shareId);
    }
    return parameters;
}
}

SearchRecipientsJob::SearchRecipientsJob(AccountPtr account,
                                         const QString &query,
                                         qint64 offset,
                                         qint64 limit,
                                         const QList<QString> &recipientTypeClasses,
                                         const std::optional<QString> &shareId)
    : UnifiedSharingRequest{std::move(account),
                            "/ocs/v2.php/apps/sharing/api/v1/recipients"_L1,
                            "GET"_ba,
                            {.parameters = searchRecipientsParameters(query, offset, limit, recipientTypeClasses, shareId), .passStatusCodes = {}, .body = {}}}
{
    setTimeout(requestTimeoutMsec);
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT recipientsFound(json.object().value("ocs"_L1).toObject().value("data"_L1).toArray());
    });
}

}
