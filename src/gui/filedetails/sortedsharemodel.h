/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QSortFilterProxyModel>
#include <qqmlregistration.h>
#include "sharemodel.h"

namespace OCC {

class SortedShareModel : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SortedShareModel(QObject *parent = nullptr);

protected:
    [[nodiscard]] bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override;

};

} // namespace OCC
