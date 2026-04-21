/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSortFilterProxyModel>
#include <qqmlintegration.h>

namespace OCC::Gui::Sharing {

class SharingFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(FilterType filterType READ filterType WRITE setFilterType NOTIFY filterTypeChanged)

public:
    enum FilterType {
        General,
        Settings,
    };
    Q_ENUM(FilterType)

    explicit SharingFilterModel(QObject *parent = nullptr);

    [[nodiscard]] FilterType filterType() const;
    void setFilterType(FilterType filterType);

Q_SIGNALS:
    void filterTypeChanged();

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    FilterType _filterType = FilterType::General;
};

}
