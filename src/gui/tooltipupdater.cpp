/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "tooltipupdater.h"

#include <QTreeView>
#include <QHelpEvent>
#include <QToolTip>
#include <QDebug>

using namespace OCC;

ToolTipUpdater::ToolTipUpdater(QTreeView* treeView)
    : QObject(treeView)
    , _treeView(treeView)
{
    connect(_treeView->model(), SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            SLOT(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    _treeView->viewport()->installEventFilter(this);
}

bool ToolTipUpdater::eventFilter(QObject* /*obj*/, QEvent* ev)
{
    if (ev->type() == QEvent::ToolTip)
    {
        QHelpEvent *helpEvent = static_cast<QHelpEvent *>(ev);
        _toolTipPos = helpEvent->globalPos();
    }
    return false;
}

void ToolTipUpdater::dataChanged(const QModelIndex& topLeft,
                                 const QModelIndex& bottomRight,
                                 const QVector<int>& roles)
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

