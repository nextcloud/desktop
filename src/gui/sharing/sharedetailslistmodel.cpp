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
    return _share.data();
}

void ShareDetailsListModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    QObject::disconnect(_shareDestroyedConnection);
    beginResetModel();
    _share = share;
    if (_share) {
        _shareDestroyedConnection = connect(_share, &QObject::destroyed, this, [this] {
            beginResetModel();
            _share = nullptr;
            endResetModel();
            Q_EMIT shareChanged();
        });
    }
    endResetModel();
    Q_EMIT shareChanged();
}
