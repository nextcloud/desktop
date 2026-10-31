/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "generatesecretjob.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace OCC::Gui::Sharing
{

GenerateSecretJob::GenerateSecretJob(AccountPtr account)
    : UnifiedSharingRequest{std::move(account), "/ocs/v2.php/apps/sharing/api/v1/secret"_L1, "GET"_ba}
{
    connect(this, &OcsJob::jobFinished, this, [this](const QJsonDocument &json, int) {
        Q_EMIT secretGenerated(json.object().value("ocs"_L1).toObject().value("data"_L1).toString());
    });
}

}
