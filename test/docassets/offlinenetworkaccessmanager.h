/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QNetworkAccessManager>
#include <atomic>

namespace OCC::DocAssets
{
class OfflineNetworkAccessManager final : public QNetworkAccessManager
{
public:
    explicit OfflineNetworkAccessManager(std::atomic_bool &requestAttempted, QObject *parent = nullptr);

protected:
    QNetworkReply *createRequest(Operation operation, const QNetworkRequest &request, QIODevice *body) override;

private:
    std::atomic_bool &_requestAttempted;
};
}
