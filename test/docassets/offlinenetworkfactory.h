/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QQmlNetworkAccessManagerFactory>
#include <atomic>

namespace OCC::DocAssets
{
class OfflineNetworkFactory final : public QQmlNetworkAccessManagerFactory
{
public:
    QNetworkAccessManager *create(QObject *parent) override;
    [[nodiscard]] bool requestAttempted() const;

private:
    std::atomic_bool _requestAttempted{false};
};
}
