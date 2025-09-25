/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2015 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    FolderStatusDelegate();

    enum datarole { FolderAliasRole = Qt::UserRole + 100,
        HeaderRole,
        FolderPathRole, // for a SubFolder it's the complete path
        FolderSecondPathRole,
        FolderConflictMsg,
        FolderErrorMsg,
        FolderInfoMsg,
        FolderSyncPaused,
        FolderStatusIconRole,
        FolderAccountConnected,

        SyncProgressOverallPercent,
        SyncProgressOverallString,
        SyncProgressItemString,

        AddButton, // 1 = enabled; 2 = disabled
        FolderSyncText,
        DataRoleCount

    };
    void paint(QPainter *, const QStyleOptionViewItem &, const QModelIndex &) const override;
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
        const QModelIndex &index) override;


    /**
     * return the position of the option button within the item
     */
    static QRect optionsButtonRect(QRect within, Qt::LayoutDirection direction);
    static QRect addButtonRect(QRect within, Qt::LayoutDirection direction);
    static QRect errorsListRect(QRect within);
    static int rootFolderHeightWithoutErrors(const QFontMetrics &fm, const QFontMetrics &aliasFm);

public slots:
    void slotStyleChanged();

private:
    void customizeStyle();

    static QString addFolderText();
    QPersistentModelIndex _pressedIndex;

    QIcon _iconMore;
};

} // namespace OCC
