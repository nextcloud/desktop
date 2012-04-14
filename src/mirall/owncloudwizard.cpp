/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
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
#include <QScrollBar>

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

#ifdef OWNCLOUD_CLIENT
    _ui.createNewOCRadioBtn->setVisible( false );
    _ui.createNewOwncloudLabel->setVisible( false );
#endif
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
    return OwncloudWizard::Page_OC_Credentials;
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

void OwncloudWizardSelectTypePage::setOCUrl( const QString& url )
{
  _ui.OCUrlLineEdit->setText( url );
}

// ======================================================================


OwncloudCredentialsPage::OwncloudCredentialsPage()
{
    _ui.setupUi(this);
    registerField( "OCUser",   _ui.OCUserEdit );
    registerField( "OCPasswd", _ui.OCPasswdEdit );
    registerField( "PwdNoLocalStore", _ui.cbPwdNoLocalStore );

    connect( _ui.OCPasswdEdit, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));

    connect( _ui.cbPwdNoLocalStore, SIGNAL(stateChanged(int)), this, SLOT(slotPwdStoreChanged(int)));
}

OwncloudCredentialsPage::~OwncloudCredentialsPage()
{

}

void OwncloudCredentialsPage::slotPwdStoreChanged( int state )
{
    _ui.OCPasswdEdit->setEnabled( state == Qt::Unchecked );
    emit completeChanged();
}

bool OwncloudCredentialsPage::isComplete() const
{
    if( _ui.cbPwdNoLocalStore->checkState() == Qt::Checked ) {
        return !(_ui.OCUserEdit->text().isEmpty());
    }
    return !(_ui.OCUserEdit->text().isEmpty() || _ui.OCPasswdEdit->text().isEmpty() );
}

void OwncloudCredentialsPage::initializePage()
{
    QString user = getenv( "USER" );
    _ui.OCUserEdit->setText( user );
}

int OwncloudCredentialsPage::nextId() const
{
  return OwncloudWizard::Page_Install;
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

int OwncloudFTPAccessPage::nextId() const
{
  return OwncloudWizard::Page_OC_Credentials;
}

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
    return OwncloudWizard::Page_OC_Credentials;
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
    _ui.ocLinkLabel->setVisible( false );
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

void OwncloudWizardResultPage::appendResultText( const QString& msg, OwncloudWizard::LogType type )
{
  if( msg.isEmpty() ) {
    _ui.resultTextEdit->clear();
  } else {
    if( type == OwncloudWizard::LogParagraph ) {
      _ui.resultTextEdit->append( msg );
    } else {
      // _ui.resultTextEdit->append( msg );
      _ui.resultTextEdit->insertPlainText(msg );
    }
    _ui.resultTextEdit->verticalScrollBar()->setValue( _ui.resultTextEdit->verticalScrollBar()->maximum() );
  }
}

void OwncloudWizardResultPage::showOCUrlLabel( const QString& url, bool show )
{
  _ui.ocLinkLabel->setText( tr("Congratulations! Your <a href=\"%1\" title=\"%1\">new ownCloud</a> is now up and running!").arg(url) );
  _ui.ocLinkLabel->setOpenExternalLinks( true );

  if( show ) {
    _ui.ocLinkLabel->setVisible( true );
  } else {
    _ui.ocLinkLabel->setVisible( false );
  }
}

// ======================================================================

/**
 * Folder wizard itself
 */

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_SelectType,     new OwncloudWizardSelectTypePage() );
    setPage(Page_Create_OC,      new CreateAnOwncloudPage() );
    setPage(Page_FTP,            new OwncloudFTPAccessPage() );
    setPage(Page_OC_Credentials, new OwncloudCredentialsPage() );
    setPage(Page_Install,        new OwncloudWizardResultPage() );

#ifdef Q_WS_MAC
    setWizardStyle( QWizard::ModernStyle );
#endif

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

    QString host = "ftp." +domain;
    OwncloudFTPAccessPage *p1 = static_cast<OwncloudFTPAccessPage*> (page( Page_FTP ));
    p1->setFTPUrl( host );
  }
  if( id == Page_Install ) {
    appendToResultWidget( QString() );
    showOCUrlLabel( false );
    if( field("connectMyOC").toBool() ) {
      // check the url and connect.
      _oCUrl = field("OCUrl").toString();
      emit connectToOCUrl( _oCUrl);
    } else if( field("createLocalOC").toBool() ) {
      qDebug() << "Connect to local!";
      emit installOCLocalhost();
    } else if( field("createNewOC").toBool() ) {
      // call in installation mode and install to ftp site.
      emit installOCServer();
    } else {
    }
  }
}

void OwncloudWizard::showOCUrlLabel( bool show )
{
  OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Install ));
  p->showOCUrlLabel( _oCUrl, show );
}

void OwncloudWizard::appendToResultWidget( const QString& msg, LogType type )
{
  OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Install ));
  p->appendResultText( msg, type );
}

void OwncloudWizard::setOCUrl( const QString& url )
{
  _oCUrl = url;
  OwncloudWizardSelectTypePage *p = static_cast<OwncloudWizardSelectTypePage*>(page( Page_SelectType ));
  p->setOCUrl( url );
}

} // end namespace
