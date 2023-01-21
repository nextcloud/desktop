/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include <QSortFilterProxyModel>

namespace OCC {

class ActivityListModel;

class SortedActivityListModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(ActivityListModel* activityListModel READ activityListModel WRITE setActivityListModel NOTIFY activityListModelChanged)

public:
    explicit SortedActivityListModel(QObject *parent = nullptr);

    [[nodiscard]] ActivityListModel *activityListModel() const;

signals:
    void activityListModelChanged();

public slots:
    void setActivityListModel(OCC::ActivityListModel *activityListModel);

protected:
    [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

private slots:
    void sortModel();
};

}
