/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "offlinenetworkfactory.h"
#include "offlinenetworkaccessmanager.h"

namespace OCC::DocAssets
{
bool OfflineNetworkFactory::requestAttempted() const
{
    return _requestAttempted.load();
}

QNetworkAccessManager *OfflineNetworkFactory::create(QObject *parent)
{
    return new OfflineNetworkAccessManager(_requestAttempted, parent);
}
}
