/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "abstractsharemodel.h"

#include "share.h"

using namespace Qt::StringLiterals;
using namespace OCC;
using namespace OCC::Gui::Sharing;

AbstractShareModel::AbstractShareModel(QObject *parent)
    : QAbstractListModel{parent}
{}

Share *AbstractShareModel::share() const
{
    return _share;
}

void AbstractShareModel::setShare(Share *share)
{
    if (_share == share) {
        return;
    }

    beginResetModel();
    _share = share;
    Q_EMIT shareChanged();
    endResetModel();
}
