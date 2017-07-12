/*
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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
#include <QStyledItemDelegate>

namespace OCC {

/**
 * @brief The FolderStatusDelegate class
 * @ingroup gui
 */
class FolderStatusDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QIcon m_moreIcon;
    FolderStatusDelegate();

    enum datarole { FolderAliasRole = Qt::UserRole + 100,
        HeaderRole,
        FolderPathRole, // for a SubFolder it's the complete path
        FolderSecondPathRole,
        FolderConflictMsg,
        FolderErrorMsg,
        FolderSyncPaused,
        FolderStatusIconRole,
        FolderAccountConnected,

        SyncProgressOverallPercent,
        SyncProgressOverallString,
        SyncProgressItemString,
        WarningCount,
        SyncRunning,

        AddButton // 1 = enabled; 2 = disabled
    };
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const Q_DECL_OVERRIDE;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const Q_DECL_OVERRIDE;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
        const QModelIndex &index) Q_DECL_OVERRIDE;


    /**
     * return the position of the option button within the item
     */
    static QRect optionsButtonRect(QRect within, Qt::LayoutDirection direction);
    static QRect errorsListRect(QRect within);
    static int rootFolderHeightWithoutErrors(const QFontMetrics &fm, const QFontMetrics &aliasFm);

private:
    static QString addFolderText();
};

} // namespace OCC
