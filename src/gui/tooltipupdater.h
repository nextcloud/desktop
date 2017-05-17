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

#include <QObject>
#include <QPoint>

class QTreeView;
class QModelIndex;

namespace OCC {

/**
 * @brief Updates tooltips of items in a QTreeView when they change.
 * @ingroup gui
 *
 * Usually tooltips are not updated as they change. Since we want to
 * use tooltips to show rapidly updating progress information, we
 * need to make sure that that information is displayed to the user
 * as it changes.
 *
 * To accomplish that, the eventFilter() stores the tooltip's position
 * and the dataChanged() slot updates the tooltip if Qt::ToolTipRole
 * gets updated while a tooltip is shown.
 */
class ToolTipUpdater : public QObject
{
    Q_OBJECT
public:
    ToolTipUpdater(QTreeView *treeView);

protected:
    bool eventFilter(QObject *obj, QEvent *ev) Q_DECL_OVERRIDE;

private slots:
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

private:
    QTreeView *_treeView;
    QPoint _toolTipPos;
};

} // namespace OCC
