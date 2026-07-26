/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "folderstatusview.h"
#include "folderstatusdelegate.h"

#include <QHeaderView>
#include <QScrollBar>
#include <QSizePolicy>
#include <QtGlobal>

namespace OCC {

FolderStatusView::FolderStatusView(QWidget *parent) : QTreeView(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

QSize FolderStatusView::sizeHint() const
{
    auto hint = QTreeView::sizeHint();
    hint.setHeight(visibleRowsHeight() + (header()->isVisible() ? header()->height() : 0) + 2 * frameWidth());
    return hint;
}

QSize FolderStatusView::minimumSizeHint() const
{
    return sizeHint();
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
        rect.setLeft(viewport()->rect().left());
        rect.setWidth(viewport()->width());
        return FolderStatusDelegate::addButtonRect(rect, layoutDirection());
    }
    return rect;
}

int FolderStatusView::visibleRowsHeight(const QModelIndex &parent) const
{
    auto height = 0;
    const auto rows = model() ? model()->rowCount(parent) : 0;
    for (auto row = 0; row < rows; ++row) {
        const auto index = model()->index(row, 0, parent);
        height += rowHeight(index);
        if (isExpanded(index)) {
            height += visibleRowsHeight(index);
        }
    }
    return height;
}

} // namespace OCC
