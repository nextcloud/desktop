/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "governancenetworkjob.h"

Q_LOGGING_CATEGORY(lcGovernance, "nextcloud.gui.governance", QtInfoMsg)

using namespace Qt::StringLiterals;

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

QString GovernanceNetworkJob::buildPath() const
{
    return u"/ocs/v2.php/apps/governance/%1/labels/%2/%3"_s.arg(apiVersionAsString(), entityTypeAsString(), entityId());
}

QString GovernanceNetworkJob::apiVersionAsString() const
{
    auto result = QString{};

    switch (_apiVersion)
    {
    case ApiVersion::Version_1:
        result = u"v1"_s;
        break;
    }

    return result;
}

QString GovernanceNetworkJob::entityTypeAsString() const
{
    auto result = QString{};

    switch (_entityType)
    {
    case EntityType::Files:
        result = u"FILES"_s;
        break;
    case EntityType::Mails:
        result = u"MAILS"_s;
        break;
    case EntityType::Custom:
        result = _customEntityType;
        break;
    }

    return result;
}

void GovernanceNetworkJob::setAccount(AccountPtr newAccount)
{
    if (_account == newAccount) {
        return;
    }

    _account = newAccount;
    Q_EMIT accountChanged();
}

} // namespace OCC
