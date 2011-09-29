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
    registerField( "connectMyOC", _ui.connectMyOCRadioBtn );
    registerField( "createNewOC", _ui.createNewOCRadioBtn );
    registerField( "OCUrl",       _ui.OCUrlLineEdit );

    connect( _ui.connectMyOCRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.createNewOCRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.OCUrlLineEdit, SIGNAL(textChanged(QString)), SIGNAL(completeChanged()));
}

OwncloudWizardSelectTypePage::~OwncloudWizardSelectTypePage()
{

}

void OwncloudWizardSelectTypePage::initializePage()
{

}

int OwncloudWizardSelectTypePage::nextId() const
{
  if( _ui.connectMyOCRadioBtn->isChecked() ) {
    return OwncloudWizard::Page_Install;
  }
  return OwncloudWizard::Page_Create_OC;
}

bool OwncloudWizardSelectTypePage::isComplete() const
{
  if( _ui.connectMyOCRadioBtn->isChecked() ) {
    // a valid url is needed.
    QString u = _ui.OCUrlLineEdit->text();
    QUrl url( u );
    if( url.isValid() ) {
      return true;
    }
    return false;
  }
  return true;
}

// ======================================================================


OwncloudFTPAccessPage::OwncloudFTPAccessPage()
{
    _ui.setupUi(this);
    registerField( "ftpUrl",    _ui.ftpUrlEdit );
    registerField( "ftpUser",   _ui.ftpUserEdit );
    registerField( "ftpPasswd", _ui.ftpPasswdEdit );
    // registerField( "ftpDir",    _ui.ftpDir );
}

OwncloudFTPAccessPage::~OwncloudFTPAccessPage()
{
}

void OwncloudFTPAccessPage::initializePage()
{
    // _ui.lineEditOCAlias->setText( "Owncloud" );
}

void OwncloudFTPAccessPage::setFTPUrl( const QString& url )
{
  _ui.ftpUrlEdit->setText( url );
}

#if 0
int OwncloudFTPAccessPage::nextId() const
{

}
#endif

bool OwncloudFTPAccessPage::isComplete() const
{

}

// ======================================================================

CreateAnOwncloudPage::CreateAnOwncloudPage()
{
    _ui.setupUi(this);
    registerField("createLocalOC",  _ui.createLocalRadioBtn );
    registerField("createOnDomain", _ui.createPerFTPRadioBtn );
    registerField("myOCDomain",     _ui.myDomainEdit );

    connect( _ui.createLocalRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.createPerFTPRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.myDomainEdit, SIGNAL(textChanged(QString)), SIGNAL(completeChanged()));
}

CreateAnOwncloudPage::~CreateAnOwncloudPage()
{
}

void CreateAnOwncloudPage::initializePage()
{
    // _ui.lineEditOCAlias->setText( "Owncloud" );
}

int CreateAnOwncloudPage::nextId() const
{
  if( _ui.createLocalRadioBtn->isChecked() ) {
    return OwncloudWizard::Page_Install;
  }

  return OwncloudWizard::Page_FTP;
}

bool CreateAnOwncloudPage::isComplete() const
{

  if( _ui.createPerFTPRadioBtn->isChecked() ) {
    QString dom = _ui.myDomainEdit->text();
    qDebug() << "check is Complete with " << dom;
    return (!dom.isEmpty() && dom.contains( QChar('.'))
            && dom.lastIndexOf('.') < dom.length()-2 );
  }
  return true;
}

QString CreateAnOwncloudPage::domain() const
{
  return _ui.myDomainEdit->text();
}
// ======================================================================

OwncloudWizardResultPage::OwncloudWizardResultPage()
{
    _ui.setupUi(this);
    // no fields to register.
    _ui.resultTextEdit->setAcceptRichText(true);
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

void OwncloudWizardResultPage::appendResultText( const QString& msg )
{
  if( msg.isEmpty() ) {
    _ui.resultTextEdit->clear();
  } else {
    _ui.resultTextEdit->append( msg );
  }
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

    setWizardStyle( QWizard::ClassicStyle );

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));

}

void OwncloudWizard::slotCurrentPageChanged( int id )
{
  qDebug() << "Current Wizard page changed to " << id;
  qDebug() << "Page_install is " << Page_Install;

  if( id == Page_FTP ) {
    // preset the ftp url field
    CreateAnOwncloudPage *p = static_cast<CreateAnOwncloudPage*> (page( Page_Create_OC ));
    QString domain = p->domain();
    if( domain.startsWith( "http://" )) {
      domain = domain.right( domain.length()-7 );
    }
    if( domain.startsWith( "https://" )) {
      domain = domain.right( domain.length()-8 );
    }
    field("myOCUrl").setValue( domain );
    QString host = "ftp." +domain;
    OwncloudFTPAccessPage *p1 = static_cast<OwncloudFTPAccessPage*> (page( Page_FTP ));
    p1->setFTPUrl( host );
  }
  if( id == Page_Install ) {
    appendToResultWidget( QString() );
    if( field("connectMyOC").toBool() ) {
      // check the url and connect.
      QString url = field("OCUrl").toString();
      emit connectToOCUrl( url );
    } else if( field("createLocalOC").toBool() ) {
      qDebug() << "Connect to local!";
      emit installOCLocalhost();
    } else if( field("createNewOC").toBool() ) {
      // call in installation mode and install to ftp site.
      emit installOCServer();
    } else {
      qDebug() << "XX: " << field("connectMyOC").toBool();
    }
  }
}

void OwncloudWizard::appendToResultWidget( const QString& msg )
{
  OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Install ));
  p->appendResultText( msg );
}

} // end namespace

#include "owncloudwizard.moc"
