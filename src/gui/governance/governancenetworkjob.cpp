/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "governancenetworkjob.h"

namespace OCC
{

GovernanceNetworkJob::GovernanceNetworkJob(QObject *parent)
    : QObject{parent}
{
}

GovernanceNetworkJob::ApiVersion GovernanceNetworkJob::apiVersion() const
{
    return _apiVersion;
}

void GovernanceNetworkJob::setApiVersion(ApiVersion newApiVersion)
{
    if (_apiVersion == newApiVersion) {
        return;
    }

    _apiVersion = newApiVersion;
    Q_EMIT apiVersionChanged();
}

GovernanceNetworkJob::EntityType GovernanceNetworkJob::entityType() const
{
    return _entityType;
}

void GovernanceNetworkJob::setEntityType(EntityType newEntityType)
{
    if (_entityType == newEntityType) {
        return;
    }

    _entityType = newEntityType;
    Q_EMIT entityTypeChanged();
}

QString GovernanceNetworkJob::customEntityType() const
{
    return _customEntityType;
}

void GovernanceNetworkJob::setCustomEntityType(const QString &newCustomEntityType)
{
    if (_customEntityType == newCustomEntityType) {
        return;
    }

    _customEntityType = newCustomEntityType;
    Q_EMIT customEntityTypeChanged();
}

QString GovernanceNetworkJob::entityId() const
{
    return _entityId;
}

void GovernanceNetworkJob::setEntityId(const QString &newEntityId)
{
    if (_entityId == newEntityId) {
        return;
    }

    _entityId = newEntityId;
    Q_EMIT entityIdChanged();
}

} // namespace OCC
