/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmodel.h"

#include "account.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

SharingModel::SharingModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int SharingModel::rowCount(const QModelIndex &parent) const
{
    if (!_accountState) {
        return 0;
    }

    // TODO: cache this after setting accountstate
    const auto sharing = accountState()->account()->sharing();
    const auto features = sharing->features().values();
    return features.size();
}

QVariant SharingModel::data(const QModelIndex &index, int role) const
{
    if (!_accountState) {
        return {};
    }

    // TODO: cache this after setting accountstate
    const auto sharing = accountState()->account()->sharing();
    const auto features = sharing->features().values();
    const auto feature = features.at(index.row());

    switch (role) {
    case LabelRole:
        return feature->type();
    case PropertyRole:
        return u"prop%1"_s.arg(QString::number(index.row()));
    case TypeRole:
        return static_cast<FieldTypes>(index.row() % 3);
    case PlaceholderRole:
        return u"Placeholder for row %1"_s.arg(QString::number(index.row()));
    default:
        return {};
    }
}

QHash<int, QByteArray> SharingModel::roleNames() const
{
    return {
        { LabelRole, "label"_ba},
        { PropertyRole, "property"_ba},
        { TypeRole, "type"_ba},
        { PlaceholderRole, "placeholder"_ba},
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

    beginResetModel();
    _accountState = accountState;
    Q_EMIT accountStateChanged();
    endResetModel();
}
