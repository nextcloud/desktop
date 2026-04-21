/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmodel.h"

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

SharingModel::SharingModel(QObject *parent)
    : QAbstractListModel{parent}
{}

int SharingModel::rowCount(const QModelIndex &parent) const
{
    return 7;
}

QVariant SharingModel::data(const QModelIndex &index, int role) const
{
    switch (role) {
    case LabelRole:
        return u"Item number %1"_s.arg(QString::number(index.row()));
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
