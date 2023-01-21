/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
#include "sharemodel.h"

namespace OCC {

class SortedShareModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(ShareModel* shareModel READ shareModel WRITE setShareModel NOTIFY shareModelChanged)

public:
    explicit SortedShareModel(QObject *parent = nullptr);

    [[nodiscard]] ShareModel *shareModel() const;

signals:
    void shareModelChanged();

public slots:
    void setShareModel(OCC::ShareModel *shareModel);

protected:
    [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

private slots:
    void sortModel();
};

} // namespace OCC
