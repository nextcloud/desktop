/*
 * Copyright (C) by Fabian Müller <fmueller@owncloud.com>
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

#include "updatefinisheddialog.h"
#include "common/asserts.h"
#include "ui_updatefinisheddialog.h"

#include <QDialogButtonBox>
#include <QPushButton>

namespace OCC {

UpdateFinishedDialog::UpdateFinishedDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::UpdateFinishedDialog)
{
    _ui->setupUi(this);

    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);

    const auto noButton = _ui->buttonBox->button(QDialogButtonBox::No);
    const auto yesButton = _ui->buttonBox->button(QDialogButtonBox::Yes);

    noButton->setText(tr("Restart later"));

    yesButton->setText(tr("Restart now"));
    yesButton->setDefault(true);
}

UpdateFinishedDialog::~UpdateFinishedDialog()
{
    delete _ui;
}

} // OCC
