/*
 * Copyright (C) 2023 by Fabian Müller <fmueller@owncloud.com>
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

#include "updatedownloadeddialog.h"
#include "theme.h"
#include "ui_updatedownloadeddialog.h"

#include <QDialogButtonBox>
#include <QPushButton>

namespace OCC {

UpdateDownloadedDialog::UpdateDownloadedDialog(QWidget *parent, const QString &statusMessage)
    : QWidget(parent)
    , _ui(new ::Ui::UpdateDownloadedDialog)
{
    _ui->setupUi(this);

    _ui->iconLabel->setPixmap(Theme::instance()->applicationIcon().pixmap(128, 128));
    _ui->iconLabel->setText(QString());

    _ui->descriptionLabel->setText(statusMessage);

    connect(_ui->buttonBox, &QDialogButtonBox::rejected, this, &UpdateDownloadedDialog::reject);
    connect(_ui->buttonBox, &QDialogButtonBox::accepted, this, &UpdateDownloadedDialog::accept);

    const auto noButton = _ui->buttonBox->button(QDialogButtonBox::No);
    const auto yesButton = _ui->buttonBox->button(QDialogButtonBox::Yes);

    noButton->setText(tr("Restart later"));

    yesButton->setText(tr("Restart now"));
    yesButton->setDefault(true);
}

void UpdateDownloadedDialog::accept()
{
    Q_EMIT accepted();
    Q_EMIT finished();
}

void UpdateDownloadedDialog::reject()
{
    Q_EMIT finished();
}
} // OCC namespace
