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

#include "accountmodalwidget.h"
#include "ui_accountmodalwidget.h"

namespace OCC {
AccountModalWidget::AccountModalWidget(const QString &title, QWidget *widget, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AccountModalWidget)
{
    ui->setupUi(this);
    ui->groupBox->setTitle(title);
    ui->groupBox->layout()->addWidget(widget);

    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this] {
        Q_EMIT accepted();
        Q_EMIT finished();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, [this] {
        Q_EMIT rejected();
        Q_EMIT finished();
    });
}
} // OCC
