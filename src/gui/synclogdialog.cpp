/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 * Copyright (C) 2015 by Klaas Freitag <freitag@owncloud.com>
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

#include "synclogdialog.h"
#include "ui_synclogdialog.h"
#include "theme.h"
#include "syncresult.h"
#include "configfile.h"
#include "capabilities.h"

#include "QProgressIndicator.h"

#include <QPushButton>


namespace OCC {

SyncLogDialog::SyncLogDialog(QWidget *parent, ProtocolWidget *protoWidget)
    : QDialog(parent)
    , _ui(new Ui::SyncLogDialog)
{
    setObjectName("SyncLogDialog"); // required as group for saveGeometry call

    _ui->setupUi(this);

    if (protoWidget) {
        _ui->logWidgetLayout->addWidget(protoWidget);
    }

    QPushButton *closeButton = _ui->buttonBox->button(QDialogButtonBox::Close);
    if (closeButton) {
        connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));
    }
}

SyncLogDialog::~SyncLogDialog()
{
}
}
