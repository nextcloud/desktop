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

#include <QtGui>
#include <QtWidgets>

#include "issueswidget.h"
#include "configfile.h"
#include "syncresult.h"
#include "syncengine.h"
#include "logger.h"
#include "theme.h"
#include "folderman.h"
#include "syncfileitem.h"
#include "folder.h"
#include "models.h"
#include "openfilemanager.h"
#include "protocolwidget.h"
#include "accountstate.h"
#include "account.h"
#include "accountmanager.h"
#include "common/syncjournalfilerecord.h"
#include "elidedlabel.h"


#include "ui_issueswidget.h"

#include <climits>

namespace {
bool persistsUntilLocalDiscovery(const OCC::ProtocolItem &data)
{
    return data.status() == OCC::SyncFileItem::Conflict
        || (data.status() == OCC::SyncFileItem::FileIgnored && data.direction() == OCC::SyncFileItem::Up);
}

}
namespace OCC {

/**
 * If more issues are reported than this they will not show up
 * to avoid performance issues around sorting this many issues.
 */

IssuesWidget::IssuesWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::IssuesWidget)
{
    _ui->setupUi(this);

    connect(ProgressDispatcher::instance(), &ProgressDispatcher::progressInfo,
        this, &IssuesWidget::slotProgressInfo);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::itemCompleted,
        this, &IssuesWidget::slotItemCompleted);
    connect(ProgressDispatcher::instance(), &ProgressDispatcher::syncError,
        this, [this](const QString &folderAlias, const QString &message, ErrorCategory) {
            auto item = SyncFileItemPtr::create();
            item->_status = SyncFileItem::NormalError;
            item->_errorString = message;
            item->_responseTimeStamp = QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8();
            _model->addProtocolItem(ProtocolItem { folderAlias, item });
        });

    _model = new ProtocolItemModel(this);
    _sortModel = new QSortFilterProxyModel(this);
    _sortModel->setSourceModel(_model);
    _ui->_tableView->setModel(_sortModel);
    connect(_ui->_tableView, &QTreeView::customContextMenuRequested, this, &IssuesWidget::slotItemContextMenu);

    _ui->_tableView->horizontalHeader()->setObjectName(QStringLiteral("ActivityErrorListHeaderV2"));
    _ui->_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    _ui->_tableView->horizontalHeader()->setSectionResizeMode(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Action), QHeaderView::Stretch);
    _ui->_tableView->horizontalHeader()->setSortIndicator(static_cast<int>(ProtocolItemModel::ProtocolItemRole::Time), Qt::DescendingOrder);

    ConfigFile cfg;
    cfg.restoreGeometryHeader(_ui->_tableView->horizontalHeader());

    connect(qApp, &QApplication::aboutToQuit, this, [this] {
        ConfigFile cfg;
        cfg.saveGeometryHeader(_ui->_tableView->horizontalHeader());
    });


    _ui->_tooManyIssuesWarning->hide();
    connect(_model, &ProtocolItemModel::rowsInserted, this, [this] {
        _ui->_tooManyIssuesWarning->setVisible(_model->isModelFull());
    });
    connect(_model, &ProtocolItemModel::modelReset, this, [this] {
        _ui->_tooManyIssuesWarning->setVisible(_model->isModelFull());
    });

    _ui->_conflictHelp->hide();
    _ui->_conflictHelp->setText(
        tr("There were conflicts. <a href=\"%1\">Check the documentation on how to resolve them.</a>")
            .arg(Theme::instance()->conflictHelpUrl()));
}

IssuesWidget::~IssuesWidget()
{
    delete _ui;
}

void IssuesWidget::slotProgressInfo(const QString &folder, const ProgressInfo &progress)
{
    if (progress.status() == ProgressInfo::Reconcile) {
        // Wipe all non-persistent entries - as well as the persistent ones
        // in cases where a local discovery was done.
        auto f = FolderMan::instance()->folder(folder);
        if (!f)
            return;
        const auto &engine = f->syncEngine();
        const auto style = engine.lastLocalDiscoveryStyle();
        _model->remove_if([&](const ProtocolItem &item) {
            if (item.folderName() != folder) {
                return false;
            }
            if (style == LocalDiscoveryStyle::FilesystemOnly) {
                return true;
            }
            if (!persistsUntilLocalDiscovery(item)) {
                return true;
            }
            // Definitely wipe the entry if the file no longer exists
            if (!QFileInfo::exists(f->path() + item.path())) {
                return true;
            }

            auto path = QFileInfo(item.path()).dir().path();
            if (path == QLatin1Char('.'))
                path.clear();

            return engine.shouldDiscoverLocally(path);
        });
    }
    if (progress.status() == ProgressInfo::Done) {
        // We keep track very well of pending conflicts.
        // Inform other components about them.
        QStringList conflicts;
        for (const auto &data : _model->rawData()) {
            if (data.folderName() == folder
                && data.status() == SyncFileItem::Conflict) {
                conflicts.append(data.path());
            }
        }
        emit ProgressDispatcher::instance()->folderConflicts(folder, conflicts);

        _ui->_conflictHelp->setHidden(Theme::instance()->conflictHelpUrl().isEmpty() || conflicts.isEmpty());
    }
}

void IssuesWidget::slotItemCompleted(const QString &folder, const SyncFileItemPtr &item)
{
    if (!item->showInIssuesTab())
        return;
    _model->addProtocolItem(ProtocolItem { folder, item });
}

void IssuesWidget::slotItemContextMenu()
{
    auto rows = _ui->_tableView->selectionModel()->selectedRows();
    for (int i = 0; i < rows.size(); ++i) {
        rows[i] = _sortModel->mapToSource(rows[i]);
    }
    ProtocolWidget::showContextMenu(this, _model, rows);
}

}
