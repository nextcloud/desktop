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

#include "mirall/owncloudwizard.h"

namespace Mirall
{

OwncloudWizardSelectTypePage::OwncloudWizardSelectTypePage()
{
    _ui.setupUi(this);
    // registerField("OCUrl*",       _ui.lineEditOCUrl);
}

OwncloudWizardSelectTypePage::~OwncloudWizardSelectTypePage()
{

}

void OwncloudWizardSelectTypePage::initializePage()
{

}

bool OwncloudWizardSelectTypePage::isComplete() const
{

}

// ======================================================================


OwncloudFTPAccessPage::OwncloudFTPAccessPage()
{
    _ui.setupUi(this);
    // registerField("OCUrl*",       _ui.lineEditOCUrl);
}

OwncloudFTPAccessPage::~OwncloudFTPAccessPage()
{
}

void OwncloudFTPAccessPage::initializePage()
{
    // _ui.lineEditOCAlias->setText( "Owncloud" );
}

bool OwncloudFTPAccessPage::isComplete() const
{

}

// ======================================================================

CreateAnOwncloudPage::CreateAnOwncloudPage()
{
    _ui.setupUi(this);
    // registerField("OCSiteAlias*", _ui.lineEditOCAlias);
}

CreateAnOwncloudPage::~CreateAnOwncloudPage()
{
}

void CreateAnOwncloudPage::initializePage()
{
    // _ui.lineEditOCAlias->setText( "Owncloud" );
}

bool CreateAnOwncloudPage::isComplete() const
{

}

// ======================================================================

OwncloudWizardResultPage::OwncloudWizardResultPage()
{
    _ui.setupUi(this);
    // registerField("OCSiteAlias*", _ui.lineEditOCAlias);
}

OwncloudWizardResultPage::~OwncloudWizardResultPage()
{
}

void OwncloudWizardResultPage::initializePage()
{
    // _ui.lineEditOCAlias->setText( "Owncloud" );
}

bool OwncloudWizardResultPage::isComplete() const
{

}

// ======================================================================

/**
 * Folder wizard itself
 */

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_SelectType, new OwncloudWizardSelectTypePage() );
    setPage(Page_Create_OC,  new CreateAnOwncloudPage() );
    setPage(Page_FTP,        new OwncloudFTPAccessPage() );
    setPage(Page_Install,    new OwncloudWizardResultPage() );
}

} // end namespace

#include "owncloudwizard.moc"
