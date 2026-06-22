/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2016 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

private:
    QTreeView *_treeView;
    QPoint _toolTipPos;
};

} // namespace OCC
