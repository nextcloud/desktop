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

#include "protocolwidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "logger.h"
#include "theme.h"
#include "folderman.h"
#include "folder.h"
#include "openfilemanager.h"
#include "guiutility.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "syncfileitem.h"

#include "models/models.h"
#include "models/activitylistmodel.h"

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
    _ui->_tableView->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(_ui->_tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, [this] {
        showHeaderContextMenu(this, _sortModel);
    });

    _model = new ProtocolItemModel(this);
    _sortModel = new QSortFilterProxyModel(this);
    _sortModel->setSourceModel(_model);
    _sortModel->setSortRole(Models::UnderlyingDataRole);
    _ui->_tableView->setModel(_sortModel);

    _ui->_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    _ui->_tableView->horizontalHeader()->setSectionResizeMode(static_cast<int>(ProtocolItemModel::ProtocolItemRole::File), QHeaderView::Stretch);
    _ui->_tableView->horizontalHeader()->setSortIndicator(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Time), Qt::DescendingOrder);

    _ui->_tableView->horizontalHeader()->setObjectName(QStringLiteral("ActivityListHeaderV2"));
    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_tableView->horizontalHeader());

    connect(qApp, &QApplication::aboutToQuit, this, [this] {
        ConfigFile cfg;
        cfg.saveGeometryHeader(_ui->_tableView->horizontalHeader());
    });

    _ui->_headerLabel->setText(tr("Local sync protocol"));
}

ProtocolWidget::~ProtocolWidget()
{
    delete _ui;
}

void ProtocolWidget::showHeaderContextMenu(QWidget *parent, QSortFilterProxyModel *model)
{
    Models::displayFilterDialog(AccountManager::instance()->accountNames().toList(), model, static_cast<int>(ProtocolItemModel::ProtocolItemRole::Account), Qt::DisplayRole, parent);
}

void ProtocolWidget::showContextMenu(QWidget *parent, ProtocolItemModel *model, const QModelIndexList &items)
{
    auto menu = new QMenu(parent);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // keep in sync with ActivityWidget::slotItemContextMenu
    menu->addAction(tr("Copy to clipboard"), parent, [text = Models::formatSelection(items)] {
        QApplication::clipboard()->setText(text);
    });

    if (items.size() == 1) {
        const auto &data = model->protocolItem(items.first());
        auto folder = FolderMan::instance()->folder(data.folderName());
        if (folder) {
            {
                const QString localPath = folder->path() + data.path();
                if (QFileInfo::exists(localPath)) {
                    // keep in sync with ActivityWidget::slotItemContextMenu
                    menu->addAction(tr("Show in file browser"), parent, [localPath] {
                        if (QFileInfo::exists(localPath)) {
                            showInFileManager(localPath);
                        }
                    });
                }
            }
            // "Open in Browser" action
            {
                fetchPrivateLinkUrl(folder->accountState()->account(), folder->remotePathTrailingSlash() + data.path(), parent, [parent, menu = QPointer<QMenu>(menu)](const QString &url) {
                    // as fetchPrivateLinkUrl is async we need to check the menu still exists
                    if (menu) {
                        menu->addAction(tr("Show in web browser"), parent, [url, parent] {
                            Utility::openBrowser(url, parent);
                        });
                    }
                });
            }
            {
                switch (data.status()) {
                case SyncFileItem::DetailError:
                    Q_FALLTHROUGH();
                case SyncFileItem::SoftError:
                    Q_FALLTHROUGH();
                case SyncFileItem::BlacklistedError:
                    menu->addAction(tr("Retry sync"), parent, [folder, &data] {
                        folder->journalDb()->wipeErrorBlacklistEntry(data.path());
                        FolderMan::instance()->scheduleFolderNext(folder);
                    });
                default:
                    break;
                }
            }
        }
    }
    menu->popup(QCursor::pos());
}

void ProtocolWidget::slotItemContextMenu()
{
    auto rows = _ui->_tableView->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        rows[i] = _sortModel->mapToSource(rows[i]);
    }
    showContextMenu(this, _model, rows);
}

void ProtocolWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (!item->showInProtocolTab())
        return;
    _model->addProtocolItem(ProtocolItem { folder, item });
}

}
