/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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
#include "spacesbrowser.h"
#include "ui_spacesbrowser.h"

#include "spacesmodel.h"

#include "graphapi/drives.h"

#include "gui/models/expandingheaderview.h"

#include <QCursor>
#include <QMenu>

using namespace OCC::Spaces;

SpacesBrowser::SpacesBrowser(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SpacesBrowser)
{
    ui->setupUi(this);
    _model = new SpacesModel(this);
    ui->tableView->setModel(_model);

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SpacesBrowser::selectionChanged);

    ui->tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    auto header = new OCC::ExpandingHeaderView(QStringLiteral("SpacesBrowserHeader"), ui->tableView);
    ui->tableView->setHorizontalHeader(header);
    header->setExpandingColumn(static_cast<int>(SpacesModel::Columns::Name));
    header->hideSection(static_cast<int>(SpacesModel::Columns::WebDavUrl));
    // not used yet
    header->hideSection(static_cast<int>(SpacesModel::Columns::WebUrl));
    // not relevant for users
    header->hideSection(static_cast<int>(SpacesModel::Columns::LocalMountPoint));
    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested, header, [header, this] {
        auto menu = new QMenu(this);
        menu->setAttribute(Qt::WA_DeleteOnClose);
        header->addResetActionToMenu(menu);
        menu->popup(QCursor::pos());
    });
}

SpacesBrowser::~SpacesBrowser()
{
    delete ui;
}

void SpacesBrowser::setAccount(OCC::AccountPtr acc)
{
    _acc = acc;
    if (acc) {
        QTimer::singleShot(0, this, [this] {
            auto drive = new OCC::GraphApi::Drives(_acc);
            connect(drive, &OCC::GraphApi::Drives::finishedSignal, [drive, this] {
                _model->setData(_acc, drive->drives());
                show();
            });
            drive->start();
        });
    }
}

QModelIndex SpacesBrowser::currentSpace()
{
    const auto spaces = ui->tableView->selectionModel()->selectedRows();
    return spaces.isEmpty() ? QModelIndex {} : spaces.first();
}
