/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
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

#include "foldercreationdialog.h"
 
#include "buttonstyle.h"
#include "ionostheme.h"
#include "ui_foldercreationdialog.h"

#include <limits>

#include <QDir>
#include <QMessageBox>
#include <QLoggingCategory>
#include <QHBoxLayout>
#include <QMetaType>
#include <QPushButton>

namespace OCC {

Q_LOGGING_CATEGORY(lcFolderCreationDialog, "hidrivenext.gui.foldercreationdialog", QtInfoMsg)

FolderCreationDialog::FolderCreationDialog(const QString &destination, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::FolderCreationDialog)
    , _destination(destination)
{
    ui->setupUi(this);
    setWindowTitle(tr("%1 Create new folder").arg(Theme::instance()->appNameGUI()));
    customizeStyle();

    ui->errorSnackbar->setVisible(false);

    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowFlags(windowFlags() | Qt::Dialog | Qt::WindowMinMaxButtonsHint);

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
        ui->errorSnackbar->setVisible(true);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        sizeDialog();

    } else {
        ui->errorSnackbar->setVisible(false);
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        sizeDialog();
    }
}

void FolderCreationDialog::sizeDialog(){
    adjustSize();
    setFixedWidth(626);
    setFixedHeight(sizeHint().height());
}

void FolderCreationDialog::customizeStyle()
{
    ui->buttonBox->setLayoutDirection(Qt::RightToLeft);

    QPushButton *okButton = ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setProperty("buttonStyle", QVariant::fromValue(OCC::ButtonStyleName::Primary));

    QHBoxLayout* buttonlayout = qobject_cast<QHBoxLayout*>(ui->buttonBox->layout());
    buttonlayout->setSpacing(16);

    ui->newFolderNameEdit->setStyleSheet(
        QStringLiteral(
            "color: %1; font-family: %2; font-size: %3; font-weight: %4; border-radius: %5; border: 1px "
            "solid %6; padding: 0px 12px; text-align: left; vertical-align: middle; height: 40px; background: %7; ")
            .arg(IonosTheme::folderWizardPathColor(),
                 IonosTheme::settingsFont(),
                 IonosTheme::settingsTextSize(),
                 IonosTheme::settingsTextWeight(),
                 IonosTheme::buttonRadius(),
                 IonosTheme::menuBorderColor(),
                 IonosTheme::white()
            )
    );

#if defined(Q_OS_MAC)
    buttonlayout->setSpacing(32);
#endif

    sizeDialog();
}
}