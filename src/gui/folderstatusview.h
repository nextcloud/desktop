/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef FOLDERSTATUSVIEW_H
#define FOLDERSTATUSVIEW_H

#include <QTreeView>

namespace OCC {

/**
 * @brief The FolderStatusView class
 * @ingroup gui
 */
class FolderStatusView : public QTreeView
{
    Q_OBJECT

public:
    explicit FolderStatusView(QWidget *parent = nullptr);

    [[nodiscard]] QModelIndex indexAt(const QPoint &point) const override;
    [[nodiscard]] QRect visualRect(const QModelIndex &index) const override;
    [[nodiscard]] QSize sizeHint() const override;
};

} // namespace OCC

#endif // FOLDERSTATUSVIEW_H
