/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmodel.h"

#include <QPromise>

#include "account.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

SharingModel::SharingModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int SharingModel::rowCount(const QModelIndex &parent) const
{
    if (!(_accountState && _share)) {
        return 0;
    }

    return _share->properties().size();
}

QVariant SharingModel::data(const QModelIndex &index, int role) const
{
    if (!(_accountState && _share)) {
        return {};
    }

    // // if (index.row() == 0) {
    // //     // special feature for the recipients search
    // //     switch (role) {
    // //     case LabelRole:
    // //         return "Add people"_L1;
    // //     case PropertyRole:
    // //         return "recipients"_L1;
    // //     case TypeRole:
    // //         return FieldTypes::RecipientsField;
    // //     case PlaceholderRole:
    // //         return "Name, team, email, or federated cloud ID"_L1;
    // //     case ValueRole:
    // //         return _fieldValues.value("recipients"_L1).toList(); // TODO
    // //     default:
    // //         return {};
    // //     }
    // // }
    // //
    // // TODO: cache this after setting accountstate
    // const auto sharing = accountState()->account()->sharing();
    // // const auto features = sharing->features().values();
    // // const auto feature = features.at(index.row() - 1); // TODO: -1 to adjust the special feature for searching
    //

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

bool SharingModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;
    if (!_accountState) {
        qCritical() << "no accountState, but setData called with" << index << value << role;
        return false;
    }

    qCritical() << "setData called with" << index << value << role;

    if (role != ValueRole) {
        return false;
    }

    if (index.row() == 0) {
        qCritical() << "changing" << "recipients"_L1 << "to" << value;
        _fieldValues.insert("recipients"_L1, value); // TODO
    } else {
        // TODO: cache this after setting accountstate
        const auto sharing = accountState()->account()->sharing();
        // const auto features = sharing->features().values();
        // const auto feature = features.at(index.row() - 1); // TODO: -1 to adjust the special feature for searching
        // qCritical() << "changing" << feature->type() << "to" << value;
        // _fieldValues.insert(feature->type(), value);
    }

    Q_EMIT dataChanged(index, index, {ValueRole});
    return true;
}

Qt::ItemFlags SharingModel::flags(const QModelIndex &index) const
{
    qCritical() << "flags for" << index << QAbstractListModel::flags(index) << Qt::ItemIsEditable << "returning" << (QAbstractListModel::flags(index) | Qt::ItemIsEditable);
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

QHash<int, QByteArray> SharingModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { PropertyRole, "property"_ba},
        { TypeRole, "type"_ba},
        { PlaceholderRole, "placeholder"_ba},
        { ValueRole, "value"_ba},
    };
};

AccountState *SharingModel::accountState() const
{
    return _accountState;
}

void SharingModel::setAccountState(AccountState *accountState)
{
    if (_accountState == accountState) {
        return;
    }

    _accountState = accountState;
    Q_EMIT accountStateChanged();

    auto job = _accountState->account()->sharing()->createShareJob(this);
    connect(job, &OCC::JsonApiJob::jsonReceived, this, [this](const QJsonDocument &json, int statusCode) -> void {
        beginResetModel();
        qCritical() << "request finished with code" << statusCode << "data" << json;
        _share = OCC::Sharing::Share::fromJson(json, _accountState->account());
        qCritical() << "share id:" << _share->id();
        endResetModel();

        connect(_share.get(), &OCC::Sharing::Share::propertiesChanged, this, [this]() -> void {
            beginResetModel();
            endResetModel();
        });

        _share->addSource("8"_L1);
    });
    job->start();
}

void SharingModel::addRecipient(const QString &type, const QString &value)
{
    _share->addRecipient(type, value);
}
