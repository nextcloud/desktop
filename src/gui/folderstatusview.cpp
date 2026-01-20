/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderstatusview.h"
#include "folderstatusdelegate.h"

#include <QScrollBar>
#include <QtGlobal>

namespace OCC {

FolderStatusView::FolderStatusView(QWidget *parent) : QTreeView(parent)
{
}

QModelIndex FolderStatusView::indexAt(const QPoint &point) const
{
    QModelIndex index = QTreeView::indexAt(point);
    if (index.data(FolderStatusDelegate::AddButton).toBool() && !visualRect(index).contains(point)) {
        return {};
    }
    return index;
}

QRect FolderStatusView::visualRect(const QModelIndex &index) const
{
    QRect rect = QTreeView::visualRect(index);
    if (index.data(FolderStatusDelegate::AddButton).toBool()) {
        return FolderStatusDelegate::addButtonRect(rect, layoutDirection());
    }
    return rect;
}

QSize FolderStatusView::sizeHint() const
{
    const auto baseHint = QTreeView::sizeHint();
    if (!model()) {
        return baseHint;
    }

    const int rowCount = model()->rowCount();
    const int rowSizeHint = rowCount > 0 ? sizeHintForRow(0) : -1;
    const int fallbackRowHeight = fontMetrics().height() + 8;
    const int rowHeight = rowSizeHint > 0 ? rowSizeHint : fallbackRowHeight;
    const int visibleRows = qMax(1, rowCount);
    int height = rowHeight * visibleRows;

    height += frameWidth() * 2;
    if (horizontalScrollBar()->isVisible()) {
        height += horizontalScrollBar()->sizeHint().height();
    }

    return {baseHint.width(), height};
}

} // namespace OCC
