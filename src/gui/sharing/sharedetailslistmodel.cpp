/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharedetailslistmodel.h"

#include "unifiedshare.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

ShareDetailsListModel::ShareDetailsListModel(QObject *parent)
    : QAbstractListModel{parent}
{}

Share *ShareDetailsListModel::share() const
{
    return _share;
}

void ShareDetailsListModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    beginResetModel();
    _share = share;
    Q_EMIT shareChanged();
    endResetModel();
}
