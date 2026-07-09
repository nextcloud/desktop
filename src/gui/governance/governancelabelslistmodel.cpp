/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "governancelabelslistmodel.h"

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

int GovernanceLabelsListModel::rowCount(const QModelIndex &parent) const
{
    auto result = 0;
    if (parent.isValid()) {
        return result;
    }

    result = _data.count() + 1;
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

    if (index.row() < 0 || index.row() >= _data.count() + 1) {
        return result;
    }

    if (role >= Qt::UserRole + 1) {
        auto convertedRole = static_cast<LabelsListModelRoles>(role);

        [[unlikely]] if (index.row() == _data.count()) {
            switch (convertedRole)
            {
            case LabelsListModelRoles::IdRole:
                result = -1;
                break;
            case LabelsListModelRoles::NameRole:
                result = tr("None");
                break;
            case LabelsListModelRoles::PriorityRole:
                result = -1;
                break;
            case LabelsListModelRoles::DescriptionRole:
                result = tr("No label");
                break;
            case LabelsListModelRoles::ColorRole:
            case LabelsListModelRoles::ScopesRole:
                break;
            case LabelsListModelRoles::SelectedRole:
                result = false;
                break;
            }
        } else {
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
            case LabelsListModelRoles::ScopesRole:
                result = _data[index.row()]._scopes;
                break;
            case LabelsListModelRoles::SelectedRole:
                result = _data[index.row()]._status == GovernanceLabelInfo::Status::Selected;
                break;
            }
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
        {static_cast<int>(LabelsListModelRoles::ScopesRole), "scopes"_ba},
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

    if (hasReceivedAllData()) {
        refreshModel();
    }
}

void GovernanceLabelsListModel::setExistingLabelsJsonData(const QJsonDocument &reply)
{
    _existingLabelsReply = reply.object();

    if (hasReceivedAllData()) {
        refreshModel();
    }
}

void GovernanceLabelsListModel::toggleLabel(const int index, const QString &labelId)
{
    if (_data.size() == index) {
        for (const auto &oneLabel : std::as_const(_data)) {
            if (oneLabel._status == GovernanceLabelInfo::Status::Selected) {
                Q_EMIT removeLabel(oneLabel._id);
            }
        }
    } else if (index <_data.size() && index >= 0) {
        switch (_data[index]._status)
        {
        case GovernanceLabelInfo::Status::Selected:
            Q_EMIT removeLabel(labelId);
            break;
        case GovernanceLabelInfo::Status::Available:
            Q_EMIT addLabel(labelId);
            break;
        case GovernanceLabelInfo::Status::UnknownStatus:
            break;
        }
    }
}

void GovernanceLabelsListModel::toggleLabel(const QString &labelId)
{
    Q_EMIT addLabel(labelId);
}

void OCC::GovernanceLabelsListModel::etagChanged()
{
    emitRefreshData();
}

void GovernanceLabelsListModel::labelWasModified()
{
    emitRefreshData();
}

void GovernanceLabelsListModel::emitRefreshData()
{
    if (_entityId.isEmpty() || _labelType == Governance::LabelType::InvalidLabelType) {
        return;
    }

    _availableLabelsReply = {};
    _existingLabelsReply = {};
    Q_EMIT refreshAvailableLabelsData(_labelType, _entityId);
    Q_EMIT refreshExistingLabelsData(_entityId);
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
    return _availableLabelsReply.contains(u"ocs"_s) && _existingLabelsReply.contains(u"ocs"_s);
}

void GovernanceLabelsListModel::refreshModel()
{
    const auto dataArray = readOcsReply(_availableLabelsReply).toArray();
    const auto dataReply = readOcsReply(_existingLabelsReply).toObject();

    const auto convertToStringList = [] (const QJsonArray &scopesList) -> QStringList
    {
        auto result = QStringList{};

        for (const auto &oneScope : scopesList) {
            result << oneScope.toString();
        }

        return result;
    };

    const auto parseExistingSingleLabel = [this] (const int rowIndex, const QJsonObject &oneLabel) -> void{
        if (_data[rowIndex]._id == oneLabel[u"id"_s]) {
            if (_data[rowIndex]._status != GovernanceLabelInfo::Status::Selected) {
                _data[rowIndex]._status = GovernanceLabelInfo::Status::Selected;
                Q_EMIT dataChanged(index(rowIndex), index(rowIndex));
            }
        } else {
            if (_data[rowIndex]._status != GovernanceLabelInfo::Status::Available) {
                _data[rowIndex]._status = GovernanceLabelInfo::Status::Available;
                Q_EMIT dataChanged(index(rowIndex), index(rowIndex));
            }
        }
    };

    beginResetModel();
    _data.clear();
    for (const auto oneLabel : dataArray) {
        const auto rowIndex = _data.size();
        const auto oneLabelObject = oneLabel.toObject();
        _data.emplaceBack(oneLabelObject.value(u"id"_s).toString(),
                          oneLabelObject.value(u"name"_s).toString(),
                          oneLabelObject.value(u"priority"_s).toInt(),
                          oneLabelObject.value(u"description"_s).toString(),
                          oneLabelObject.value(u"color"_s).toString(),
                          convertToStringList(oneLabelObject.value(u"scopes"_s).toArray())
                          );

        switch (_labelType)
        {
        case Governance::LabelType::Sensitivity:
        {
            if (!dataReply.contains(u"sensitivity"_s)) {
                qCWarning(lcGovernanceLabelsListModel()) << "missing sensitivity data in OCS reply";
                break;
            }

            const auto &sensitivityLabel = dataReply[u"sensitivity"_s].toObject();

            parseExistingSingleLabel(rowIndex, sensitivityLabel);
            break;
        }
        case Governance::LabelType::Retention:
        {
            if (!dataReply.contains(u"retention"_s)) {
                qCWarning(lcGovernanceLabelsListModel()) << "missing retention data in OCS reply";
                break;
            }

            const auto retentionLabels = dataReply[u"retention"_s].toArray();

            for (const auto &oneLabel : retentionLabels) {
                const auto &oneLabelObject = oneLabel.toObject();
                parseExistingSingleLabel(rowIndex, oneLabelObject);
            }
            break;
        }
        case Governance::LabelType::LegalHold:
        {
            if (!dataReply.contains(u"hold"_s)) {
                qCWarning(lcGovernanceLabelsListModel()) << "missing legal hold data in OCS reply";
                break;
            }

            const auto legalHoldLabels = dataReply[u"hold"_s].toArray();

            for (const auto &oneLabel : legalHoldLabels) {
                const auto &oneLabelObject = oneLabel.toObject();
                parseExistingSingleLabel(rowIndex, oneLabelObject);
            }
            break;
        }
        case Governance::LabelType::InvalidLabelType:
            break;
        }
    }
    endResetModel();
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

} // namespace OCC
