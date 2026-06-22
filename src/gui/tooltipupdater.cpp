/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tooltipupdater.h"

#include <QTreeView>
#include <QHelpEvent>
#include <QToolTip>

using namespace OCC;

ToolTipUpdater::ToolTipUpdater(QTreeView *treeView)
    : QObject(treeView)
    , _treeView(treeView)
{
    connect(_treeView->model(), &QAbstractItemModel::dataChanged,
        this, &ToolTipUpdater::dataChanged);
    _treeView->viewport()->installEventFilter(this);
}

bool ToolTipUpdater::eventFilter(QObject * /*obj*/, QEvent *ev)
{
    if (ev->type() == QEvent::ToolTip) {
        auto *helpEvent = dynamic_cast<QHelpEvent *>(ev);
        _toolTipPos = helpEvent->globalPos();
    }
    return false;
}

void ToolTipUpdater::dataChanged(const QModelIndex &topLeft,
    const QModelIndex &bottomRight,
    const QVector<int> &roles)
{
    if (!QToolTip::isVisible() || !roles.contains(Qt::ToolTipRole) || _toolTipPos.isNull()) {
        return;
    }

    // Was it the item under the cursor that changed?
    auto index = _treeView->indexAt(_treeView->mapFromGlobal(QCursor::pos()));
    if (topLeft == bottomRight && index != topLeft) {
        return;
    }

    // Update the currently active tooltip
    QToolTip::showText(_toolTipPos, _treeView->model()->data(index, Qt::ToolTipRole).toString());
}
