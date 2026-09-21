/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QNetworkAccessManager>
#include <QStringList>

namespace OCC::DocAssets
{
class FixtureNetworkAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;
    QHash<QUrl, QByteArray> getResponses;
    QStringList requests;
    QStringList missingFixtures;

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *body) override;
};
}
