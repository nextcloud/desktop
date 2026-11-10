/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "offlinenetworkaccessmanager.h"
#include <QNetworkRequest>

namespace OCC::DocAssets
{
OfflineNetworkAccessManager::OfflineNetworkAccessManager(std::atomic_bool &requestAttempted, QObject *parent)
    : QNetworkAccessManager(parent)
    , _requestAttempted(requestAttempted)
{
}

QNetworkReply *OfflineNetworkAccessManager::createRequest(Operation, const QNetworkRequest &, QIODevice *)
{
    _requestAttempted.store(true);
    // Qt's missing-resource reply fails asynchronously without opening a network connection.
    return QNetworkAccessManager::createRequest(GetOperation, QNetworkRequest(QUrl(QStringLiteral("qrc:/docassets/blocked-request"))), nullptr);
}
}
