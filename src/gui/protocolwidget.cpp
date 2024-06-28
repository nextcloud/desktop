/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include <QCursor>
#include <QtGui>
#include <QtWidgets>

#include "accountmanager.h"
#include "accountstate.h"
#include "commonstrings.h"
#include "folder.h"
#include "folderman.h"
#include "guiutility.h"
#include "openfilemanager.h"
#include "protocolwidget.h"
#include "syncfileitem.h"

#include "models/expandingheaderview.h"

#include "ui_protocolwidget.h"

namespace OCC {

ProtocolWidget::ProtocolWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::ProtocolWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &ProtocolWidget::slotItemCompleted);

    connect(_ui->_tableView, &QTreeWidget::customContextMenuRequested, this, &ProtocolWidget::slotItemContextMenu);

    // Build the model-view "stack":
    //  _model <- _sortModel <- _statusSortModel <- _tableView
    _model = new ProtocolItemModel(2000, false, this);
    _sortModel = new Models::SignalledQSortFilterProxyModel(this);
    connect(_sortModel, &Models::SignalledQSortFilterProxyModel::filterChanged, this, &ProtocolWidget::filterDidChange);
    _sortModel->setSourceModel(_model);
    _sortModel->setSortRole(Models::UnderlyingDataRole);
    _ui->_tableView->setModel(_sortModel);

    auto header = new ExpandingHeaderView(QStringLiteral("ActivityListHeaderV2"), _ui->_tableView);
    _ui->_tableView->setHorizontalHeader(header);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setExpandingColumn(static_cast<int>(ProtocolItemModel::ProtocolItemRole::File));
    header->setSortIndicator(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Time), Qt::DescendingOrder);
    header->hideSection(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Status));
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested, header, [header, this] {
        auto menu = showFilterMenu(header, _sortModel, static_cast<int>(ProtocolItemModel::ProtocolItemRole::Account), tr("Account"));
        menu->addSeparator();
        header->addResetActionToMenu(menu);
    });

    connect(_ui->_filterButton, &QAbstractButton::clicked, this, [this] {
        showFilterMenu(_ui->_filterButton, _sortModel, static_cast<int>(ProtocolItemModel::ProtocolItemRole::Account), tr("Account"));
    });

    connect(FolderMan::instance(), &FolderMan::folderRemoved, this, [this](Folder *f) {
        _model->remove_if([f](const ProtocolItem &item) {
            return item.folder() == f;
        });
    });
}

ProtocolWidget::~ProtocolWidget()
{
    delete _ui;
}

/**
 * @brief Show a filter menu for the given model.
 *
 * @param parent Parent widget
 * @param model The model that will do the filtering
 * @param role the role (column number) to filter on
 * @param columnName the name column on which the filter is done
 * @return
 */
QMenu *ProtocolWidget::showFilterMenu(QWidget *parent, Models::SignalledQSortFilterProxyModel *model, int role, const QString &columnName)
{
    auto menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAccessibleName(tr("Filter menu"));
    Models::addFilterMenuItems(menu, AccountManager::instance()->accountNames(), model, role, columnName, Qt::DisplayRole);
    QTimer::singleShot(0, menu, [menu] {
        menu->popup(QCursor::pos());
        // accassability
        menu->setFocus();
    });
    return menu;
}

void ProtocolWidget::showContextMenu(QWidget *parent, QTableView *table, Models::SignalledQSortFilterProxyModel *sortModel, ProtocolItemModel *itemModel,
    const QModelIndexList &items, const QPoint &pos)
{
    auto menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setAccessibleName(tr("Actions menu"));

    // keep in sync with ActivityWidget::slotItemContextMenu
    menu->addAction(CommonStrings::copyToClipBoard(), parent, [text = Models::formatSelection(items)] {
        QApplication::clipboard()->setText(text);
    });

    if (items.size() == 1) {
        const auto &data = itemModel->protocolItem(items.first());

        // Show in file browser action
        const QString localPath = data.folder()->path() + data.path();
        // keep in sync with ActivityWidget::slotItemContextMenu
        auto showInFileBrowserAction = menu->addAction(CommonStrings::showInFileBrowser(), parent, [localPath] {
            // Double-check if the file still exists
            if (QFileInfo::exists(localPath)) {
                showInFileManager(localPath);
            }
        });
        if (!QFileInfo::exists(localPath)) {
            showInFileBrowserAction->setEnabled(false);
        }

        // "Open in Browser" action
        auto showInWebBrowserAction = menu->addAction(CommonStrings::showInWebBrowser());
        showInWebBrowserAction->setEnabled(false);
        fetchPrivateLinkUrl(data.folder()->accountState()->account(), data.folder()->webDavUrl(), data.folder()->remotePathTrailingSlash() + data.path(),
            parent, [showInWebBrowserAction, parent, pos = menu->actions().size(), menu = QPointer<QMenu>(menu)](const QUrl &url) {
                // as fetchPrivateLinkUrl is async we need to check the menu still exists
                if (menu) {
                    connect(showInWebBrowserAction, &QAction::triggered, [url, parent] { Utility::openBrowser(url, parent); });
                    showInWebBrowserAction->setEnabled(true);
                }
            });

        menu->addSeparator();

        // Sort actions
        auto sortActions = new QActionGroup(menu);
        int columnNr = table->columnAt(pos.x());
        QString columnName = itemModel->headerData(columnNr, Qt::Horizontal, Qt::DisplayRole).toString();
        auto ascendingAction =
            menu->addAction(tr("Sort ascending by %1").arg(columnName), [table, columnNr]() { table->sortByColumn(columnNr, Qt::AscendingOrder); });
        ascendingAction->setCheckable(true);
        sortActions->addAction(ascendingAction);
        auto descendingAction =
            menu->addAction(tr("Sort descending by %1").arg(columnName), [table, columnNr]() { table->sortByColumn(columnNr, Qt::DescendingOrder); });
        descendingAction->setCheckable(true);
        sortActions->addAction(descendingAction);
        if (columnNr == sortModel->sortColumn()) {
            switch (sortModel->sortOrder()) {
            case Qt::AscendingOrder:
                ascendingAction->setChecked(true);
                break;
            case Qt::DescendingOrder:
                descendingAction->setChecked(true);
                break;
            }
        }

        // Retry action
        switch (data.status()) {
        case SyncFileItem::DetailError:
            Q_FALLTHROUGH();
        case SyncFileItem::SoftError:
            Q_FALLTHROUGH();
        case SyncFileItem::BlacklistedError:
            menu->addSeparator();
            menu->addAction(tr("Retry sync"), parent, [data, folder = QPointer<Folder>(data.folder())] {
                if (folder) {
                    folder->journalDb()->wipeErrorBlacklistEntry(data.path());
                    FolderMan::instance()->scheduler()->enqueueFolder(folder, SyncScheduler::Priority::Medium);
                }
            });
            break;
        default:
            break;
        }
    }

    menu->popup(table->mapToGlobal(pos));
    menu->setFocus(); // For accassability
}

void ProtocolWidget::slotItemContextMenu(const QPoint &pos)
{
    auto rows = _ui->_tableView->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        rows[i] = _sortModel->mapToSource(rows[i]);
    }
    showContextMenu(this, _ui->_tableView, _sortModel, _model, rows, pos);
}

void ProtocolWidget::slotItemCompleted(Folder *folder, const SyncFileItemPtr &item)
{
    if (!item->showInProtocolTab())
        return;
    _model->addProtocolItem(ProtocolItem { folder, item });
}

void ProtocolWidget::filterDidChange()
{
    _ui->_filterButton->setText(CommonStrings::filterButtonText(_sortModel->filterRegularExpression().pattern().isEmpty() ? 0 : 1));
}

} // OCC namespace
