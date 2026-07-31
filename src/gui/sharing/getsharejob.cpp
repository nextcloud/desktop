/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "getsharejob.h"

#include "share.h"

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

namespace
{
std::optional<QJsonObject> getShareBody(const std::optional<QString> &secret,
                                        const std::optional<QJsonObject> &arguments)
{
    if (!secret && !arguments) {
        return std::nullopt;
    }

    auto body = QJsonObject{};
    if (secret) {
        body.insert("secret"_L1, *secret);
    }
    if (arguments) {
        body.insert("arguments"_L1, *arguments);
    }
    return body;
}
}

GetShareJob::GetShareJob(AccountPtr account,
                         const QString &shareId,
                         const std::optional<QString> &secret,
                         const std::optional<QJsonObject> &arguments)
    : UnifiedSharingRequest{account,
                            "/ocs/v2.php/apps/sharing/api/v1/share/%1"_L1.arg(shareId),
                            "POST"_ba,
                            {.body = getShareBody(secret, arguments)}}
{
    connect(this, &OcsJob::jobFinished, this, [this, account = std::move(account)](const QJsonDocument &json, int) {
        Q_EMIT shareFetched(Share::fromJson(json, account));
    });
}

}
