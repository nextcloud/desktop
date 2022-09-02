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

#include "graphapi/drives.h"

#include "gui/models/expandingheaderview.h"
#include "spaceitemwidget.h"

#include <QCursor>
#include <QMenu>

using namespace OCC::Spaces;

SpacesBrowser::SpacesBrowser(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::SpacesBrowser)
{
    _ui->setupUi(this);

    connect(_ui->listWidget, &QListWidget::itemSelectionChanged, this, [this]() {
        Q_EMIT selectionChanged();
    });

    // this way, we just need to emit selectionChanged, and have this code be called anyway
    connect(this, &SpacesBrowser::selectionChanged, this, [this]() {
        for (int i = 0; i < _ui->listWidget->count(); ++i) {
            qobject_cast<SpaceItemWidget *>(_ui->listWidget->itemWidget(_ui->listWidget->item(i)))->setRadioButtonChecked(false);
        }

        const auto selectedItems = _ui->listWidget->selectedItems();

        Q_ASSERT(selectedItems.size() == 1);
        const auto itemWidget = qobject_cast<SpaceItemWidget *>(_ui->listWidget->itemWidget(selectedItems.front()));

        itemWidget->setRadioButtonChecked(true);
    });
}

void SpacesBrowser::setItems(const AccountPtr &accountPtr, const QList<OCC::Spaces::Space> &spaces)
{
    for (const auto &space : spaces) {
        auto listWidgetItem = new QListWidgetItem(_ui->listWidget);

        _ui->listWidget->addItem(listWidgetItem);

        auto itemWidget = new SpaceItemWidget(accountPtr, space, _ui->listWidget);
        _ui->listWidget->setItemWidget(listWidgetItem, itemWidget);

        // otherwise, the widget will collapse to its minimum size
        // note that this expands the frame the list widget draws around the frame, which can only be done here
        listWidgetItem->setSizeHint(itemWidget->sizeHint());

        connect(itemWidget, &SpaceItemWidget::radioButtonClicked, this, [this, listWidgetItem]() {
            listWidgetItem->setSelected(true);
            Q_EMIT selectionChanged();
        });
    }
}

SpacesBrowser::~SpacesBrowser()
{
    delete _ui;
}

std::optional<Space> SpacesBrowser::selectedSpace() const
{
    const auto selectedItems = _ui->listWidget->selectedItems();

    if (selectedItems.empty()) {
        return std::nullopt;
    }

    Q_ASSERT(selectedItems.size() == 1);
    return qobject_cast<SpaceItemWidget *>(_ui->listWidget->itemWidget(selectedItems.front()))->space();
}
