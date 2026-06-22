/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "foldercreationdialog.h"
#include "ui_foldercreationdialog.h"

#include <limits>

#include <QDir>
#include <QMessageBox>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderCreationDialog, "nextcloud.gui.foldercreationdialog", QtInfoMsg)

FolderCreationDialog::FolderCreationDialog(const QString &destination, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FolderCreationDialog)
    , _destination(destination)
{
    ui->setupUi(this);

    ui->labelErrorMessage->setVisible(false);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(ui->newFolderNameEdit, &QLineEdit::textChanged, this, &FolderCreationDialog::slotNewFolderNameEditTextEdited);

    const QString suggestedFolderNamePrefix = QObject::tr("New folder");

    const QString newFolderFullPath = _destination + QLatin1Char('/') + suggestedFolderNamePrefix;
    if (!QDir(newFolderFullPath).exists()) {
        ui->newFolderNameEdit->setText(suggestedFolderNamePrefix);
    } else {
        for (unsigned int i = 2; i < std::numeric_limits<unsigned int>::max(); ++i) {
            const QString suggestedPostfix = QStringLiteral(" (%1)").arg(i);

            if (!QDir(newFolderFullPath + suggestedPostfix).exists()) {
                ui->newFolderNameEdit->setText(suggestedFolderNamePrefix + suggestedPostfix);
                break;
            }
        }
    }

    ui->newFolderNameEdit->setFocus();
    ui->newFolderNameEdit->selectAll();
}

FolderCreationDialog::~FolderCreationDialog()
{
    delete ui;
}

void FolderCreationDialog::accept()
{
    Q_ASSERT(!_destination.endsWith('/'));

    const auto fullPath = QString(_destination + "/" + ui->newFolderNameEdit->text());

    if (QDir(fullPath).exists()) {
        ui->labelErrorMessage->setVisible(true);
        return;
    }

    if (QDir(_destination).mkdir(ui->newFolderNameEdit->text())) {
        Q_EMIT folderCreated(fullPath);
    } else {
        QMessageBox::critical(this, tr("Error"), tr("Could not create a folder! Check your write permissions."), QMessageBox::Ok);
    }

    QDialog::accept();
}

void FolderCreationDialog::slotNewFolderNameEditTextEdited()
{
    if (!ui->newFolderNameEdit->text().isEmpty() && QDir(_destination + "/" + ui->newFolderNameEdit->text()).exists()) {
        ui->labelErrorMessage->setVisible(true);
    } else {
        ui->labelErrorMessage->setVisible(false);
    }
}

}
