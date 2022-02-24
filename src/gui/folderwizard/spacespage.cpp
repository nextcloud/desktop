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
#include "spacespage.h"
#include "ui_spacespage.h"

#include <QModelIndex>

using namespace OCC;

SpacesPage::SpacesPage(AccountPtr acc, QWidget *parent)
    : QWizardPage(parent)
    , ui(new Ui::SpacesPage)
{
    ui->setupUi(this);

    ui->widget->setAccount(acc);

    connect(ui->widget, &Spaces::SpacesBrowser::selectionChanged, this, &QWizardPage::completeChanged);
}

SpacesPage::~SpacesPage()
{
    delete ui;
}

bool OCC::SpacesPage::isComplete() const
{
    return ui->widget->currentSpace().isValid();
}

QVariant OCC::SpacesPage::selectedSpace(Spaces::SpacesModel::Columns column) const
{
    return ui->widget->currentSpace().siblingAtColumn(static_cast<int>(column)).data();
}
