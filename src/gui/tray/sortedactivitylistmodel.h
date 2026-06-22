/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSortFilterProxyModel>

namespace OCC {

class ActivityListModel;

class SortedActivityListModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit SortedActivityListModel(QObject *parent = nullptr);

protected:
    [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;
};

}
