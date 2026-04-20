/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingmodel.h"

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

int SharingModel::rowCount(const QModelIndex &parent) const
{
    return 2;
}

QVariant SharingModel::data(const QModelIndex &index, int role) const
{
    return "test item";
}

QHash<int, QByteArray> SharingModel::roleNames() const
{
    return { { LabelRole, "label"_ba} };
};
