// SPDX-FileCopyrightText: 2022 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>
#include "sharemodel.h"

namespace OCC {

class SortedShareModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit SortedShareModel(QObject *parent = nullptr);

protected:
    [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

};

} // namespace OCC
