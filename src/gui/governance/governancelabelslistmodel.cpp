/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "governancelabelslistmodel.h"

#include "applygovernancelabel.h"
#include "deletegovernancelabel.h"

#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace OCC
{

Q_LOGGING_CATEGORY(lcGovernanceLabelsListModel, "nextcloud.gui.governance.labelslistmodel", QtInfoMsg)

GovernanceLabelsListModel::GovernanceLabelsListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

AccountPtr GovernanceLabelsListModel::account() const
{
    return _account;
}

void GovernanceLabelsListModel::setAccount(AccountPtr newAccount)
{
    if (_account == newAccount) {
        return;
    }

    _account = newAccount;
    Q_EMIT accountChanged();
}

Governance::ApiVersion GovernanceLabelsListModel::apiVersion() const
{
    return _apiVersion;
}

void GovernanceLabelsListModel::setApiVersion(Governance::ApiVersion newApiVersion)
{
    if (_apiVersion == newApiVersion) {
        return;
    }

    _apiVersion = newApiVersion;
    Q_EMIT apiVersionChanged();
}

Governance::EntityType GovernanceLabelsListModel::entityType() const
{
    return _entityType;
}

void GovernanceLabelsListModel::setEntityType(Governance::EntityType newEntityType)
{
    if (_entityType == newEntityType) {
        return;
    }

    _entityType = newEntityType;
    Q_EMIT entityTypeChanged();
}

int GovernanceLabelsListModel::rowCount(const QModelIndex &parent) const
{
    auto result = 0;
    if (parent.isValid()) {
        return result;
    }

    result = _data.count();
    return result;
}

QVariant GovernanceLabelsListModel::data(const QModelIndex &index, int role) const
{
    auto result = QVariant{};

    if (!index.isValid()) {
        return result;
    }

    if (index.column() != 0) {
        return result;
    }

    if (index.row() < 0 || index.row() >= _data.count()) {
        return result;
    }

    if (role >= Qt::UserRole + 1) {
        auto convertedRole = static_cast<LabelsListModelRoles>(role);

        switch (convertedRole)
        {
        case LabelsListModelRoles::IdRole:
            result = _data[index.row()]._id;
            break;
        case LabelsListModelRoles::NameRole:
            result = _data[index.row()]._name;
            break;
        case LabelsListModelRoles::PriorityRole:
            result = _data[index.row()]._priority;
            break;
        case LabelsListModelRoles::DescriptionRole:
            result = _data[index.row()]._description;
            break;
        case LabelsListModelRoles::ColorRole:
            result = _data[index.row()]._color;
            break;
        case LabelsListModelRoles::SelectedRole:
            result = _data[index.row()]._currentStatus == GovernanceLabelInfo::Status::Selected;
            break;
        }
    }

    return result;
}

QHash<int, QByteArray> GovernanceLabelsListModel::roleNames() const
{
    auto result = QHash<int, QByteArray>{
        {static_cast<int>(LabelsListModelRoles::IdRole), "id"_ba},
        {static_cast<int>(LabelsListModelRoles::NameRole), "name"_ba},
        {static_cast<int>(LabelsListModelRoles::PriorityRole), "priority"_ba},
        {static_cast<int>(LabelsListModelRoles::DescriptionRole), "description"_ba},
        {static_cast<int>(LabelsListModelRoles::ColorRole), "color"_ba},
        {static_cast<int>(LabelsListModelRoles::SelectedRole), "isSelected"_ba},
    };

    return result;
}

Governance::LabelType GovernanceLabelsListModel::labelType() const
{
    return _labelType;
}

void GovernanceLabelsListModel::setLabelType(Governance::LabelType newLabelType)
{
    if (_labelType == newLabelType) {
        return;
    }

    _labelType = newLabelType;
    Q_EMIT labelTypeChanged();

    emitRefreshData();
}

QString GovernanceLabelsListModel::entityId() const
{
    return _entityId;
}

void GovernanceLabelsListModel::setEntityId(const QString &newEntityId)
{
    if (_entityId == newEntityId) {
        return;
    }

    _entityId = newEntityId;
    Q_EMIT entityIdChanged();

    emitRefreshData();
}

void GovernanceLabelsListModel::setAvailableLabelsJsonData(const QJsonDocument &reply)
{
    _availableLabelsReply = reply.object();

    if (!validState()) {
        qCWarning(lcGovernanceLabelsListModel()) << "setting data in an invalid model state";
        return;
    }

    if (hasReceivedAllData()) {
        refreshModel();
    }
}

void GovernanceLabelsListModel::toggleLabel(const int row)
{
    const auto toggleSingleLabel = [this] (const int row, auto &oneLabel) -> void
    {
        switch (oneLabel._currentStatus)
        {
        case GovernanceLabelInfo::Status::Selected:
            oneLabel._currentStatus = GovernanceLabelInfo::Status::Available;
            Q_EMIT dataChanged(index(row), index(row), {static_cast<int>(LabelsListModelRoles::SelectedRole)});
            setHasPendingChanges(checksForPendingChanges());
            break;
        case GovernanceLabelInfo::Status::Available:
            oneLabel._currentStatus = GovernanceLabelInfo::Status::Selected;
            Q_EMIT dataChanged(index(row), index(row), {static_cast<int>(LabelsListModelRoles::SelectedRole)});
            setHasPendingChanges(checksForPendingChanges());
            break;
        case GovernanceLabelInfo::Status::UnknownStatus:
            break;
        }
    };

    if (row < _data.size() && row >= 0) {
        auto &oneLabel = _data[row];
        toggleSingleLabel(row, oneLabel);
    } else {
        qCWarning(lcGovernanceLabelsListModel()) << "impossible to toggle the selected state of a label with invalid index" << row;
    }
}

void OCC::GovernanceLabelsListModel::etagChanged()
{
    emitRefreshData();
}

void GovernanceLabelsListModel::labelsWereModified()
{
    if (_applyingChanges || !_pendingRefreshLabelIds.isEmpty()) {
        return;
    }

    setHasPendingChanges(false);
    emitRefreshData();
    _busy = false;
    Q_EMIT busyChanged();
}

void GovernanceLabelsListModel::reset()
{
    for (auto dataRow = 0; dataRow < _data.size(); ++dataRow) {
        auto &oneLabel = _data[dataRow];
        if (oneLabel._currentStatus != oneLabel._serverStatus) {
            oneLabel._currentStatus = oneLabel._serverStatus;
            Q_EMIT dataChanged(index(dataRow), index(dataRow), {static_cast<int>(LabelsListModelRoles::SelectedRole)});
        }
    }

    setHasPendingChanges(false);
}

void GovernanceLabelsListModel::apply()
{
    _busy = true;
    Q_EMIT busyChanged();
    _applyingChanges = true;
    auto hasPendingChanges = false;

    for (auto dataRow = 0; dataRow < _data.size(); ++dataRow) {
        auto &oneLabel = _data[dataRow];
        if (oneLabel._currentStatus != oneLabel._serverStatus) {
            hasPendingChanges = true;

            switch (oneLabel._currentStatus)
            {
            case GovernanceLabelInfo::Status::Selected:
                _pendingRefreshLabelIds.insert(oneLabel._id);
                addLabel(oneLabel._id);
                break;
            case GovernanceLabelInfo::Status::Available:
                _pendingRefreshLabelIds.insert(oneLabel._id);
                removeLabel(oneLabel._id);
                break;
            case GovernanceLabelInfo::Status::UnknownStatus:
                break;
            }
        }
    }

    _applyingChanges = false;
    if (hasPendingChanges) {
        labelsWereModified();
    } else {
        _busy = false;
        Q_EMIT busyChanged();
    }
}

void GovernanceLabelsListModel::emitRefreshData()
{
    if (!validState()) {
        qCWarning(lcGovernanceLabelsListModel()) << "cannot emit refreshData signal from an invalid model state";
        return;
    }

    _availableLabelsReply = {};
    Q_EMIT refreshAvailableLabelsData(_labelType, _entityId);
}

QJsonValue GovernanceLabelsListModel::readOcsReply(const QJsonObject &replyObject)
{
    auto result = QJsonValue{};

    if (!replyObject.contains(u"ocs"_s)) {
        qCWarning(lcGovernanceLabelsListModel()) << "wrong format for reply" << replyObject;
        return result;
    }

    const auto ocsObject = replyObject.value(u"ocs"_s).toObject();

    if (!ocsObject.contains(u"data"_s)) {
        qCWarning(lcGovernanceLabelsListModel()) << "wrong format for reply" << ocsObject;
        return result;
    }

    result = ocsObject.value(u"data"_s);

    return result;
}

bool GovernanceLabelsListModel::hasReceivedAllData() const
{
    return _availableLabelsReply.contains(u"ocs"_s);
}

bool GovernanceLabelsListModel::validState() const
{
    return !_entityId.isEmpty() && _labelType != Governance::LabelType::InvalidLabelType && _account;
}

void GovernanceLabelsListModel::refreshModel()
{
    const auto availableLabelsArray = readOcsReply(_availableLabelsReply).toArray();

    qCDebug(lcGovernanceLabelsListModel()) << "label type:" << _labelType
                                           << "entity id:" << _entityId
                                           << "label behavior:" << _labelBehavior
                                           << "data:" << availableLabelsArray;

    beginResetModel();
    _data.clear();
    for (const auto oneLabel : availableLabelsArray) {
        const auto oneLabelObject = oneLabel.toObject();
        _data.emplaceBack(oneLabelObject.value(u"id"_s).toString(),
                          oneLabelObject.value(u"name"_s).toString(),
                          oneLabelObject.value(u"priority"_s).toInt(),
                          oneLabelObject.value(u"description"_s).toString(),
                          oneLabelObject.value(u"color"_s).toString(),
                          oneLabelObject.value(u"isAssigned"_s).toBool() ? GovernanceLabelInfo::Status::Selected : GovernanceLabelInfo::Status::Available,
                          oneLabelObject.value(u"isAssigned"_s).toBool() ? GovernanceLabelInfo::Status::Selected : GovernanceLabelInfo::Status::Available
                          );

        auto labelKeyName = QString{};
        switch (_labelType)
        {
        case Governance::LabelType::Sensitivity:
            labelKeyName = u"sensitivity"_s;
            break;
        case Governance::LabelType::Retention:
            labelKeyName = u"retention"_s;
            break;
        case Governance::LabelType::LegalHold:
            labelKeyName = u"hold"_s;
            break;
        case Governance::LabelType::InvalidLabelType:
            break;
        }

        qCDebug(lcGovernanceLabelsListModel()) << "label type:" << _labelType
                                               << "entity id:" << _entityId
                                               << "checking existing:" << labelKeyName
                                               << "current label:" << oneLabelObject;
    }

    _availableLabelsReply = {};

    endResetModel();
    Q_EMIT isEmptyChanged();
    setHasPendingChanges(false);
}

void GovernanceLabelsListModel::addLabel(const QString &labelId)
{
    auto addLabelJob = std::make_unique<ApplyGovernanceLabel>();

    connect(addLabelJob.get(), &ApplyGovernanceLabel::finished,
            addLabelJob.get(), [this, &labelId, addLabelJobPtr = addLabelJob.get()] ([[maybe_unused]] QJsonDocument reply) -> void
            {
                _pendingRefreshLabelIds.remove(labelId);
                labelsWereModified();
                addLabelJobPtr->deleteLater();
            });

    connect(addLabelJob.get(), &ApplyGovernanceLabel::finishedWithError,
            addLabelJob.get(), [this, &labelId, addLabelJobPtr = addLabelJob.get()] ([[maybe_unused]] int errorCode, const QString &errorMessage) -> void
            {
                _pendingRefreshLabelIds.remove(labelId);
                Q_EMIT displayError(errorMessage);
                labelsWereModified();
                addLabelJobPtr->deleteLater();
            });

    addLabelJob->setAccount(account());
    addLabelJob->setLabelType(labelType());
    addLabelJob->setEntityId(entityId());

    addLabelJob->start(labelId);
    addLabelJob.release();
}

void GovernanceLabelsListModel::removeLabel(const QString &labelId)
{
    auto addLabelJob = std::make_unique<DeleteGovernanceLabel>();

    connect(addLabelJob.get(), &ApplyGovernanceLabel::finished,
            addLabelJob.get(), [this, &labelId, addLabelJobPtr = addLabelJob.get()] ([[maybe_unused]] QJsonDocument reply) -> void
            {
                _pendingRefreshLabelIds.remove(labelId);
                labelsWereModified();
                addLabelJobPtr->deleteLater();
            });

    connect(addLabelJob.get(), &ApplyGovernanceLabel::finishedWithError,
            addLabelJob.get(), [this, &labelId, addLabelJobPtr = addLabelJob.get()] ([[maybe_unused]] int errorCode, const QString &errorMessage) -> void
            {
                _pendingRefreshLabelIds.remove(labelId);
                Q_EMIT displayError(errorMessage);
                labelsWereModified();
                addLabelJobPtr->deleteLater();
            });

    addLabelJob->setAccount(account());
    addLabelJob->setLabelType(labelType());
    addLabelJob->setEntityId(entityId());

    addLabelJob->start(labelId);
    addLabelJob.release();
}

void GovernanceLabelsListModel::setHasPendingChanges(bool newValue)
{
    if (_hasPendingChanges == newValue) {
        return;
    }

    _hasPendingChanges = newValue;
    Q_EMIT hasPendingChangesChanged();
}

bool GovernanceLabelsListModel::checksForPendingChanges() const
{
    auto hasPendingChanges = false;

    for (auto dataRow = 0; dataRow < _data.size(); ++dataRow) {
        auto &oneLabel = _data[dataRow];
        if (oneLabel._currentStatus != oneLabel._serverStatus) {
            hasPendingChanges = true;
            break;
        }
    }

    return hasPendingChanges;
}

GovernanceLabelsListModel::LabelBehavior GovernanceLabelsListModel::labelBehavior() const
{
    return _labelBehavior;
}

void GovernanceLabelsListModel::setLabelBehavior(LabelBehavior newLabelBehavior)
{
    if (_labelBehavior == newLabelBehavior) {
        return;
    }

    _labelBehavior = newLabelBehavior;
    Q_EMIT labelBehaviorChanged();
}

bool GovernanceLabelsListModel::busy() const
{
    return _busy;
}

bool GovernanceLabelsListModel::isEmpty() const
{
    return _data.isEmpty();
}

bool GovernanceLabelsListModel::hasPendingChanges() const
{
    return _hasPendingChanges;
}

} // namespace OCC
