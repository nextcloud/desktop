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

SearchRecipientsJob::SearchRecipientsJob(AccountPtr account, const QString &query, qint64 offset, qint64 limit)
    : UnifiedSharingRequest{std::move(account),
                            "/ocs/v2.php/apps/sharing/api/v1/recipients"_L1,
                            "GET"_ba,
                            {{"query"_L1, query}, {"offset"_L1, QString::number(offset)}, {"limit"_L1, QString::number(limit)}}}
{
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT recipientsFound(json.object().value("ocs"_L1).toObject().value("data"_L1).toArray());
    });
}

}
