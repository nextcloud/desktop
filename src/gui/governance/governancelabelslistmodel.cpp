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
        case LabelsListModelRoles::ScopesRole:
            result = _data[index.row()]._scopes;
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
        {static_cast<int>(LabelsListModelRoles::ScopesRole), "scopes"_ba},
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
    const auto replyObject = reply.object();

    if (!replyObject.contains(u"ocs"_s)) {
        qCWarning(lcGovernanceLabelsListModel()) << "wrong format for reply" << reply.toJson(QJsonDocument::JsonFormat::Compact);
        return;
    }

    const auto ocsObject = replyObject.value(u"ocs"_s).toObject();

    if (!ocsObject.contains(u"data"_s)) {
        qCWarning(lcGovernanceLabelsListModel()) << "wrong format for reply" << ocsObject;
        return;
    }

    const auto dataArray = ocsObject.value(u"data"_s).toArray();

    const auto convertToStringList = [] (const QJsonArray &scopesList) -> QStringList
    {
        auto result = QStringList{};

        for (const auto &oneScope : scopesList) {
            result << oneScope.toString();
        }

        return result;
    };

    beginResetModel();
    _data.clear();
    for (const auto oneLabel : dataArray) {
        const auto oneLabelObject = oneLabel.toObject();
        _data.emplaceBack(oneLabelObject.value(u"id"_s).toString(),
                          oneLabelObject.value(u"name"_s).toString(),
                          oneLabelObject.value(u"priority"_s).toInt(),
                          oneLabelObject.value(u"description"_s).toString(),
                          oneLabelObject.value(u"color"_s).toString(),
                          convertToStringList(oneLabelObject.value(u"scopes"_s).toArray())
                          );
    }
    endResetModel();
}

void GovernanceLabelsListModel::setExistingLabelsJsonData(const QJsonDocument &data)
{
    qCInfo(lcGovernanceLabelsListModel()) << data.toJson(QJsonDocument::JsonFormat::Compact);
}

void OCC::GovernanceLabelsListModel::etagChanged()
{
    Q_EMIT refreshData(_labelType, _entityId);
}

void GovernanceLabelsListModel::emitRefreshData()
{
    if (_entityId.isEmpty() || _labelType == Governance::LabelType::Invalid) {
        return;
    }

    Q_EMIT refreshData(_labelType, _entityId);
}

} // namespace OCC
