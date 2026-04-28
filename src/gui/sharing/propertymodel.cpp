/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propertymodel.h"

#include <QPointer>

#include "share.h"
#include "property.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

PropertyModel::PropertyModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int PropertyModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)

    if (!_share) {
        return 0;
    }

    return _share->properties().size();
}

QVariant PropertyModel::data(const QModelIndex &index, int role) const
{
    if (!_share) {
        return {};
    }

    const auto properties = _share->properties();
    const auto property = properties.at(index.row());

    switch (role) {
    case LabelRole:
        return property->displayName();
    case PropertyRole:
        return property->className();
    case TypeRole:
        if (property->className() == "string"_L1) {
            return FieldTypes::TextField;
        }
        // TODO: date etc.
        return FieldTypes::TextField;
    case PlaceholderRole:
        return property->hint();
    case ValueRole:
        // return _fieldValues.value(feature->type());
        return property->value();
    default:
        return {};
    }
}

bool PropertyModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;

    qCritical() << "setData called with" << index << value << role;

    if (role != ValueRole) {
        return false;
    }

    if (index.row() == 0) {
        qCritical() << "changing" << "recipients"_L1 << "to" << value;
        _fieldValues.insert("recipients"_L1, value); // TODO
    } else {
        // TODO: cache this after setting accountstate
        // const auto sharing = account()->account()->sharing();
        // const auto features = sharing->features().values();
        // const auto feature = features.at(index.row() - 1); // TODO: -1 to adjust the special feature for searching
        // qCritical() << "changing" << feature->type() << "to" << value;
        // _fieldValues.insert(feature->type(), value);
    }

    Q_EMIT dataChanged(index, index, {ValueRole});
    return true;
}

Qt::ItemFlags PropertyModel::flags(const QModelIndex &index) const
{
    qCritical() << "flags for" << index << QAbstractListModel::flags(index) << Qt::ItemIsEditable << "returning" << (QAbstractListModel::flags(index) | Qt::ItemIsEditable);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> PropertyModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { PropertyRole, "property"_ba},
        { TypeRole, "type"_ba},
        { PlaceholderRole, "placeholder"_ba},
        { ValueRole, "value"_ba},
    };
};

Share *PropertyModel::share() const
{
    return _share;
}

void PropertyModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    _share = share;
    Q_EMIT shareChanged();
}
