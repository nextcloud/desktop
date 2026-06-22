/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
