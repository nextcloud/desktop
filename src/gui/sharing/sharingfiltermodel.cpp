/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingfiltermodel.h"
#include "sharingmodel.h"

using namespace Qt::StringLiterals;
using namespace OCC::Gui::Sharing;

SharingFilterModel::SharingFilterModel(QObject *parent)
    : QSortFilterProxyModel{parent}
{}


SharingFilterModel::FilterType SharingFilterModel::filterType() const
{
    return _filterType;
}

void SharingFilterModel::setFilterType(SharingFilterModel::FilterType filterType)
{
    if (_filterType == filterType) {
        return;
    }

    beginFilterChange();
    _filterType = filterType;
    endFilterChange();
    Q_EMIT filterTypeChanged();
}

bool SharingFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto model = qobject_cast<SharingModel *>(sourceModel());
    if (!model) {
        // not a SharingModel, no need to filter
        return true;
    }

    const auto index = model->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
        return true;
    }

    auto propertyRole = model->data(index, SharingModel::PropertyRole).toString();
    bool filterProperty = false;
    if (propertyRole == "prop2" || propertyRole == "prop3" || propertyRole == "prop5") {
        filterProperty = true;
    }

    switch (_filterType) {
    case General:
        return !filterProperty;
    case Settings:
        return filterProperty;
    }

    return true;
}
