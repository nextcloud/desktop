/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "sharingfiltermodel.h"
#include "sharingmodel.h"

#include "account.h"

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

QStringList SharingFilterModel::recipientTypes() const
{
    return _recipientTypes;
}

void SharingFilterModel::setRecipientTypes(const QStringList &recipientTypes)
{
    if (_recipientTypes == recipientTypes) {
        return;
    }

    beginFilterChange();
    _recipientTypes = recipientTypes;
    endFilterChange();
    Q_EMIT recipientTypesChanged();
}

bool SharingFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const auto model = qobject_cast<SharingModel *>(sourceModel());
    if (!model) {
        // not a SharingModel, no need to filter
        return true;
    }

    const auto accountState = model->accountState();
    if (!accountState) {
        return true;
    }

    const auto index = model->index(sourceRow, 0, sourceParent);
    if (!index.isValid()) {
        return true;
    }

    const auto sharing = accountState->account()->sharing();
    const auto featureRole = model->data(index, SharingModel::LabelRole).toString(); // TODO: this will not be label
    if (featureRole == "Add people"_L1) {
        // TODO: same as above for now; >1 because link shares are only one recipient types
        return _recipientTypes.size() > 1;
    }

    if (!sharing->isFeatureAvailable(featureRole, {"OCA\\Files\\Sharing\\SourceType\\NodeShareSourceType"}, _recipientTypes)) {
        // this feature is not available in general for these recipient types
        return false;
    }

    bool isAdvancedProperty = !featureRole.contains("NoteShareFeature");

    switch (_filterType) {
    case General:
        return !isAdvancedProperty;
    case Settings:
        return isAdvancedProperty;
    }

    return true;
}
