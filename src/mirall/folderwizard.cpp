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
#include <QDir>
#include <stdlib.h>

#include "mirall/folderwizard.h"

namespace Mirall
{

FolderWizardSourcePage::FolderWizardSourcePage()
{
    _ui.setupUi(this);
    registerField("sourceFolder*", _ui.localFolderLineEdit);
    _ui.localFolderLineEdit->setText( QString( "%1/%2").arg( QDir::homePath() ).arg("Owncloud" ) );
    registerField("alias*", _ui.aliasLineEdit);
    _ui.aliasLineEdit->setText( QString::fromLatin1("Owncloud") );
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

    registerField("local?",            _ui.localFolderRadioBtn);
    registerField("remote?",           _ui.urlFolderRadioBtn);
    registerField("OC?",               _ui.OCRadioBtn);
    registerField("targetLocalFolder", _ui.localFolder2LineEdit);
    registerField("targetURLFolder",   _ui.urlFolderLineEdit);
    registerField("targetOCFolder",    _ui.OCFolderLineEdit);
}

FolderWizardTargetPage::~FolderWizardTargetPage()
{

}

bool FolderWizardTargetPage::isComplete() const
{
    if (_ui.localFolderRadioBtn->isChecked()) {
        return QFileInfo(_ui.localFolder2LineEdit->text()).isDir();
    } else if (_ui.urlFolderRadioBtn->isChecked()) {
        QUrl url(_ui.urlFolderLineEdit->text());
        return url.isValid() && (url.scheme() == "sftp" || url.scheme() == "smb");
    } else if( _ui.OCRadioBtn->isChecked()) {
        return true;
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

void FolderWizardTargetPage::on_urlFolderRadioBtn_toggled()
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

void FolderWizardTargetPage::on_urlFolderLineEdit_textChanged()
{
    emit completeChanged();
}

void FolderWizardTargetPage::slotToggleItems()
{
    bool enabled = _ui.localFolderRadioBtn->isChecked();
    _ui.localFolder2LineEdit->setEnabled(enabled);
    _ui.localFolder2ChooseBtn->setEnabled(enabled);

    enabled = _ui.urlFolderRadioBtn->isChecked();
    _ui.urlFolderLineEdit->setEnabled(enabled);

    enabled = _ui.OCRadioBtn->isChecked();
    _ui.OCFolderLineEdit->setEnabled(enabled);
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

FolderWizardNetworkPage::FolderWizardNetworkPage()
{
    _ui.setupUi(this);
    registerField("onlyNetwork*", _ui.checkBoxOnlyOnline);
    registerField("onlyLocalNetwork*", _ui.checkBoxOnlyThisLAN );
}

FolderWizardNetworkPage::~FolderWizardNetworkPage()
{
}

bool FolderWizardNetworkPage::isComplete() const
{
    return true;
}

FolderWizardOwncloudPage::FolderWizardOwncloudPage()
{
    _ui.setupUi(this);
    registerField("OCUrl*",       _ui.lineEditOCUrl);
    registerField("OCUser*",      _ui.lineEditOCUser );
    registerField("OCPasswd",     _ui.lineEditOCPasswd);
    registerField("OCSiteAlias*", _ui.lineEditOCAlias);
}

FolderWizardOwncloudPage::~FolderWizardOwncloudPage()
{
}

void FolderWizardOwncloudPage::initializePage()
{
    _ui.lineEditOCAlias->setText( "Owncloud" );
    _ui.lineEditOCUrl->setText( "http://localhost/owncloud" );
    QString user( getenv("USER"));
    _ui.lineEditOCUser->setText( user );
}

bool FolderWizardOwncloudPage::isComplete() const
{

    bool hasAlias = !(_ui.lineEditOCAlias->text().isEmpty());
    QUrl u( _ui.lineEditOCUrl->text() );
    bool hasUrl   = u.isValid();

    return hasAlias && hasUrl;
}

/**
 * Folder wizard itself
 */

FolderWizard::FolderWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_Source, new FolderWizardSourcePage());
    setPage(Page_Target, new FolderWizardTargetPage());
    setPage(Page_Network, new FolderWizardNetworkPage());
    setPage(Page_Owncloud, new FolderWizardOwncloudPage());
}

} // end namespace

#include "folderwizard.moc"
