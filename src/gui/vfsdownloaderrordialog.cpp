/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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
#include "vfsdownloaderrordialog.h"
#include "ui_vfsdownloaderrordialog.h"

namespace OCC {

VfsDownloadErrorDialog::VfsDownloadErrorDialog(const QString &fileName, const QString &errorMessage, QWidget *parent)
    : QDialog(parent)
    , _ui(new Ui::VfsDownloadErrorDialog) 
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    _ui->setupUi(this);
    _ui->descriptionLabel->setText(tr("Error downloading %1").arg(fileName));
    _ui->explanationLabel->setText(tr("%1 could not be downloaded.").arg(fileName));
    _ui->moreDetailsLabel->setText(errorMessage);
    _ui->moreDetailsLabel->setVisible(false);

    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

VfsDownloadErrorDialog::~VfsDownloadErrorDialog() = default;
}
