/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>

#include "mirall/folderwizard.h"

namespace Mirall
{

FolderWizardSourcePage::FolderWizardSourcePage()
{
    _ui.setupUi(this);
    registerField("sourceFolder*", _ui.localFolderLineEdit);
    registerField("alias*", _ui.aliasLineEdit);
}

FolderWizardSourcePage::~FolderWizardSourcePage()
{
}

bool FolderWizardSourcePage::isComplete() const
{
    return QFileInfo(_ui.localFolderLineEdit->text()).isDir();
}

void FolderWizardSourcePage::on_localFolderChooseBtn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the source folder"),
                                                    QDesktopServices::storageLocation(QDesktopServices::HomeLocation));
    if (!dir.isEmpty()) {
        _ui.localFolderLineEdit->setText(dir);
    }
}

void FolderWizardSourcePage::on_localFolderLineEdit_textChanged()
{
    emit completeChanged();
}

FolderWizardTargetPage::FolderWizardTargetPage()
{
    _ui.setupUi(this);

    registerField("local?", _ui.localFolderRadioBtn);
    registerField("remote?", _ui.sshFolderRadioBtn);

    registerField("targetLocalFolder", _ui.localFolder2LineEdit);
    registerField("targetSSHFolder", _ui.sshFolderLineEdit);
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{
}

bool FolderWizardTargetPage::isComplete() const
{
    if (_ui.localFolderRadioBtn->isChecked()) {
        return QFileInfo(_ui.localFolder2LineEdit->text()).isDir();
    }
    else if (_ui.sshFolderRadioBtn->isChecked()) {
        QUrl url(_ui.sshFolderLineEdit->text());
        return url.isValid() && url.scheme() == "ssh";
    }
    return false;
}

void FolderWizardTargetPage::initializePage()
{
    slotToggleItems();
}

void FolderWizardTargetPage::on_localFolderRadioBtn_toggled()
{
    slotToggleItems();
    emit completeChanged();
}

void FolderWizardTargetPage::on_sshFolderRadioBtn_toggled()
{
    slotToggleItems();
    emit completeChanged();

}

void FolderWizardTargetPage::on_checkBoxOnlyOnline_toggled()
{
    slotToggleItems();
}

void FolderWizardTargetPage::on_localFolder2LineEdit_textChanged()
{
    emit completeChanged();
}

void FolderWizardTargetPage::on_sshFolderLineEdit_textChanged()
{
    emit completeChanged();
}

void FolderWizardTargetPage::slotToggleItems()
{
    bool enabled = _ui.localFolderRadioBtn->isChecked();
    _ui.localFolder2LineEdit->setEnabled(enabled);
    _ui.localFolder2ChooseBtn->setEnabled(enabled);

    enabled = _ui.sshFolderRadioBtn->isChecked();
    _ui.sshFolderLineEdit->setEnabled(enabled);
    _ui.checkBoxOnlyOnline->setEnabled(enabled);
    _ui.checkBoxOnlyThisLAN->setEnabled(enabled);

    _ui.checkBoxOnlyThisLAN->setEnabled(_ui.checkBoxOnlyOnline->isEnabled() &&
                                        _ui.checkBoxOnlyOnline->isChecked());
}

void FolderWizardTargetPage::on_localFolder2ChooseBtn_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    tr("Select the target folder"),
                                                    QDesktopServices::storageLocation(QDesktopServices::HomeLocation));
    if (!dir.isEmpty()) {
        _ui.localFolder2LineEdit->setText(dir);
    }
}

/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_Source, new FolderWizardSourcePage());
    setPage(Page_Target, new FolderWizardTargetPage());
}

}

#include "folderwizard.moc"
