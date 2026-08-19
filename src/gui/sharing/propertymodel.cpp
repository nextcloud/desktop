/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propertymodel.h"

#include "unifiedshare.h"
#include "property.h"

#include <algorithm>

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

PropertyModel::PropertyModel(QObject *parent)
    : ShareDetailsListModel{parent}
{}

int PropertyModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    return _properties.size();
}

QVariant PropertyModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= _properties.size()) {
        return {};
    }

    const auto property = _properties.at(index.row());

    switch (role) {
    case LabelRole:
        return property->displayName();
    case PropertyRole:
        return property->className();
    case TypeRole: {
        static const auto fieldTypes = QHash<QString, FieldTypes>{
            {"boolean"_L1, FieldTypes::Boolean},
            {"date"_L1, FieldTypes::Date},
            {"enum"_L1, FieldTypes::Enum},
            {"password"_L1, FieldTypes::Password},
            {"string"_L1, FieldTypes::String},
        };
        return fieldTypes.value(property->type(), FieldTypes::Unknown);
    }
    case PlaceholderRole:
        return property->hint();
    case ValueRole:
        return property->value();
    case RequiredRole:
        return property->required();
    case AdvancedRole:
        return property->advanced();
    case ValidValuesRole:
        return property->validValues();
    case MinimumRole:
        return property->type() == "date"_L1 ? property->minDate() : property->minLength();
    case MaximumRole:
        return property->type() == "date"_L1 ? property->maxDate() : property->maxLength();
    default:
        return {};
    }
}

QHash<int, QByteArray> PropertyModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { PropertyRole, "property"_ba},
        { TypeRole, "type"_ba},
        { PlaceholderRole, "placeholder"_ba},
        { ValueRole, "value"_ba},
        { RequiredRole, "required"_ba},
        { AdvancedRole, "advanced"_ba},
        { ValidValuesRole, "validValues"_ba},
        { MinimumRole, "minimum"_ba},
        { MaximumRole, "maximum"_ba},
    };
}

void PropertyModel::setShare(Share *share)
{
    ShareDetailsListModel::setShare(share);
    resetProperties();

    if (!_share) {
        return;
    }

    connect(_share, &Share::propertiesChanged, this, [this]() -> void {
        resetProperties();
    });
}

bool PropertyModel::advanced() const
{
    return _advanced;
}

void PropertyModel::setAdvanced(bool advanced)
{
    if (_advanced == advanced) {
        return;
    }

    _advanced = advanced;
    resetProperties();
    Q_EMIT advancedChanged();
}

void PropertyModel::resetProperties()
{
    beginResetModel();
    _properties.clear();
    if (_share) {
        const auto properties = _share->properties();
        std::ranges::copy_if(properties, std::back_inserter(_properties), [this](const auto &property) {
            return property->advanced() == _advanced;
        });
        std::sort(_properties.begin(), _properties.end(), [](const auto &left, const auto &right) {
            return left->priority() > right->priority();
        });
    }
    endResetModel();
}
