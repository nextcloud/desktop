/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fixturenetworkaccessmanager.h"
#include "fixturereply.h"
#include "searchresponses.h"
#include <QLoggingCategory>

namespace OCC::DocAssets
{
Q_LOGGING_CATEGORY(lcFixtureNetwork, "nextcloud.docassets.network")
QNetworkReply *FixtureNetworkAccessManager::createRequest(Operation operation, const QNetworkRequest &request, QIODevice *)
{
    requests.append(request.url().toString());
    const auto isGet = operation == GetOperation
        || (operation == CustomOperation && request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == QByteArrayLiteral("GET"));
    const auto payload = isGet ? (getResponses.contains(request.url()) ? getResponses.value(request.url()) : searchResponse(request.url())) : QByteArray{};
    if (payload.isNull()) {
        missingFixtures.append(request.url().toString());
        qCWarning(lcFixtureNetwork) << "Unmatched fixture request" << request.url();
    }
    return new FixtureReply(operation, request, payload, "application/json", this);
}
}
