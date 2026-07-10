/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "governancenetworkjob.h"

using namespace Qt::StringLiterals;

namespace OCC
{

Q_LOGGING_CATEGORY(lcGovernanceNetwork, "nextcloud.gui.governance.network", QtInfoMsg)

GovernanceNetworkJob::GovernanceNetworkJob(QObject *parent)
    : QObject{parent}
{
}

Governance::ApiVersion GovernanceNetworkJob::apiVersion() const
{
    return _apiVersion;
}

void GovernanceNetworkJob::setApiVersion(Governance::ApiVersion newApiVersion)
{
    if (_apiVersion == newApiVersion) {
        return;
    }

    _apiVersion = newApiVersion;
    Q_EMIT apiVersionChanged();
}

Governance::EntityType GovernanceNetworkJob::entityType() const
{
    return _entityType;
}

void GovernanceNetworkJob::setEntityType(Governance::EntityType newEntityType)
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

QString GovernanceNetworkJob::integerEntityIdAsString() const
{
    auto result = u"-1"_s;

    const auto entityIdView = QStringView{_entityId};
    const auto idView = entityIdView.left(8);
    auto conversionStatus = false;
    const auto fileId = idView.toInt(&conversionStatus);
    if (conversionStatus) {
        result = QString::number(fileId);
    }

    return result;
}

void GovernanceNetworkJob::setEntityId(const QString &newEntityId)
{
    if (_entityId == newEntityId) {
        return;
    }

    _entityId = newEntityId;
    Q_EMIT entityIdChanged();
}

QString GovernanceNetworkJob::apiVersionAsString() const
{
    auto result = QString{};

    switch (_apiVersion)
    {
    case Governance::ApiVersion::Version_1:
        result = u"v1"_s;
        break;
    case Governance::ApiVersion::InvalidApiVersion:
        result = u"invalid"_s;
        break;
    }

    return result;
}

QString GovernanceNetworkJob::entityTypeAsString() const
{
    auto result = QString{};

    switch (_entityType)
    {
    case Governance::EntityType::Files:
        result = u"FILES"_s;
        break;
    case Governance::EntityType::Mails:
        result = u"MAILS"_s;
        break;
    case Governance::EntityType::Custom:
        result = _customEntityType;
        break;
    }

    return result;
}

bool GovernanceNetworkJob::checkParameters() const
{
    auto result = true;

    if (!_account) {
        result = false;
        return result;
    }

    if (_apiVersion == Governance::ApiVersion::InvalidApiVersion) {
        result = false;
        return result;
    }

    if (_entityType != Governance::EntityType::Files) {
        result = false;
        return result;
    }

    if (_entityType == Governance::EntityType::Custom && _customEntityType.isEmpty()) {
        result = false;
        return result;
    }

    if (_entityId.isEmpty()) {
        result = false;
        return result;
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

void GovernanceNetworkJob::classBegin()
{
}

void GovernanceNetworkJob::componentComplete()
{
    if (checkParameters()) {
        start();
    }
}

void GovernanceNetworkJob::jobDone(QJsonDocument reply, [[maybe_unused]] int statusCode)
{
    Q_EMIT finished(reply);
}

} // namespace OCC
