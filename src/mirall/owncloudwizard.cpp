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
#include "mirall/owncloudwizard.h"
#include "mirall/mirallconfigfile.h"

#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QValidator>
#include <QWizardPage>
#include <QDir>
#include <QScrollBar>
#include <QSslSocket>

#include <stdlib.h>

namespace Mirall
{

void setupCustomMedia( QVariant variant, QLabel *label )
{
    if( ! label ) return;

    QPixmap pix = variant.value<QPixmap>();
    if( ! pix.isNull() ) {
        label->setPixmap(pix);
        label->setAlignment( Qt::AlignTop | Qt::AlignRight );
        label->setVisible(true);
    } else {
        QString str = variant.toString();
        if( !str.isEmpty() ) {
            label->setText( str );
            label->setTextFormat( Qt::RichText );
            label->setVisible(true);
            label->setOpenExternalLinks(true);
        }
    }
}

// ======================================================================


OwncloudSetupPage::OwncloudSetupPage()
{
    _ui.setupUi(this);

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(handleNewOcUrl(QString)));

    registerField( QLatin1String("OCUrl"), _ui.leUrl );
    registerField( QLatin1String("OCUser"),   _ui.leUsername );
    registerField( QLatin1String("OCPasswd"), _ui.lePassword);
    registerField( QLatin1String("connectMyOC"), _ui.cbConnectOC );
    registerField( QLatin1String("secureConnect"), _ui.cbSecureConnect );
    registerField( QLatin1String("PwdNoLocalStore"), _ui.cbNoPasswordStore );

    _ui.cbSecureConnect->setEnabled(QSslSocket::supportsSsl());

    connect( _ui.lePassword, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));

    connect( _ui.cbNoPasswordStore, SIGNAL(stateChanged(int)), this, SLOT(slotPwdStoreChanged(int)));
    connect( _ui.cbSecureConnect, SIGNAL(stateChanged(int)), this, SLOT(slotSecureConChanged(int)));

    _ui.cbConnectOC->hide();
    setupCustomization();

#if QT_VERSION >= 0x040700
    _ui.leUsername->setPlaceholderText(tr("john"));
    _ui.lePassword->setPlaceholderText(tr("secret"));
#endif
}

OwncloudSetupPage::~OwncloudSetupPage()
{
}

void OwncloudSetupPage::setOCUrl( const QString& newUrl )
{
    QString url( newUrl );
    if( url.isEmpty() ) {
        _ui.leUrl->clear();
        return;
    }
    if( url.startsWith( QLatin1String("https"))) {
        _ui.cbSecureConnect->setChecked( true );
        url.remove(0,5);
    } else if( url.startsWith( QLatin1String("http"))) {
        _ui.cbSecureConnect->setChecked( false );
        url.remove(0,4);
    }
    if( url.startsWith( QLatin1String("://"))) url.remove(0,3);

    _ui.leUrl->setText( url );
}

void OwncloudSetupPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.sideLabel->setText( QString::null );
    _ui.sideLabel->setFixedWidth(160);

    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    MirallConfigFile cfg;

    QVariant variant = cfg.customMedia( MirallConfigFile::oCSetupTop );
    setupCustomMedia( variant, _ui.topLabel );
    variant = cfg.customMedia( MirallConfigFile::oCSetupSide );
    setupCustomMedia( variant, _ui.sideLabel );
    variant = cfg.customMedia( MirallConfigFile::oCSetupBottom );
    setupCustomMedia( variant, _ui.bottomLabel );

    QString fixUrl = cfg.customMedia( MirallConfigFile::oCSetupFixUrl ).toString();
    if( !fixUrl.isEmpty() ) {
        setOCUrl( fixUrl );
        _ui.leUrl->setEnabled( false );
        _ui.cbSecureConnect->hide();
        _ui.leUrl->hide();
        _ui.protocolLabel->hide();
        _ui.serverAddressLabel->hide();
    }
}

void OwncloudSetupPage::slotPwdStoreChanged( int state )
{
    _ui.lePassword->setEnabled( state == Qt::Unchecked );
    emit completeChanged();
}

void OwncloudSetupPage::slotSecureConChanged( int state )
{
    if( state == Qt::Checked ) {
        _ui.protocolLabel->setText(QLatin1String("https://"));
    } else {
        _ui.protocolLabel->setText(QLatin1String("http://"));
    }
}

void OwncloudSetupPage::handleNewOcUrl(const QString& ocUrl)
{
    QUrl url(ocUrl);

    QString urlMinusScheme = url.toString(QUrl::RemoveScheme);

    // QUrl::RemoveScheme leaves the beginning slashes. Remove them
    // if they're present.
    if (urlMinusScheme.startsWith("//")) {
        urlMinusScheme.remove(0, 2);
    }

    _ui.leUrl->setText(urlMinusScheme);
}

bool OwncloudSetupPage::isComplete() const
{
    if( _ui.leUrl->text().isEmpty() ) return false;

    if( _ui.cbNoPasswordStore->checkState() == Qt::Checked ) {
        return !(_ui.leUsername->text().isEmpty());
    }
    return !(_ui.leUsername->text().isEmpty() || _ui.lePassword->text().isEmpty() );
}

void OwncloudSetupPage::initializePage()
{
    QString user = QString::fromLocal8Bit(qgetenv( "USER" ));
    _ui.leUsername->setText( user );
}

int OwncloudSetupPage::nextId() const
{
  return OwncloudWizard::Page_Install;
}

// ======================================================================

OwncloudWizardSelectTypePage::OwncloudWizardSelectTypePage()
{
    _ui.setupUi(this);
    registerField( QLatin1String("connectMyOC"), _ui.connectMyOCRadioBtn );
    registerField( QLatin1String("createNewOC"), _ui.createNewOCRadioBtn );
    registerField( QLatin1String("OCUrl"),       _ui.OCUrlLineEdit );

    connect( _ui.connectMyOCRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.createNewOCRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.OCUrlLineEdit, SIGNAL(textChanged(QString)), SIGNAL(completeChanged()));

#ifdef OWNCLOUD_CLIENT
    _ui.createNewOCRadioBtn->setVisible( false );
    _ui.createNewOwncloudLabel->setVisible( false );
#endif

#if QT_VERSION >= 0x040700
    _ui.OCUrlLineEdit->setPlaceholderText(tr("http://owncloud.mydomain.org"));
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
    registerField( QLatin1String("OCUser"),   _ui.OCUserEdit );
    registerField( QLatin1String("OCPasswd"), _ui.OCPasswdEdit );
    registerField( QLatin1String("PwdNoLocalStore"), _ui.cbPwdNoLocalStore );

    connect( _ui.OCPasswdEdit, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));

    connect( _ui.cbPwdNoLocalStore, SIGNAL(stateChanged(int)), this, SLOT(slotPwdStoreChanged(int)));

#if QT_VERSION >= 0x040700
    _ui.OCUserEdit->setPlaceholderText(tr("john"));
    _ui.OCPasswdEdit->setPlaceholderText(tr("secret"));
#endif
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
    QString user = QString::fromLocal8Bit(qgetenv( "USER" ));
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
    registerField( QLatin1String("ftpUrl"),    _ui.ftpUrlEdit );
    registerField( QLatin1String("ftpUser"),   _ui.ftpUserEdit );
    registerField( QLatin1String("ftpPasswd"), _ui.ftpPasswdEdit );
    // registerField( QLatin1String("ftpDir"),    _ui.ftpDir );

#if QT_VERSION >= 0x040700
    _ui.ftpUrlEdit->setPlaceholderText(tr("ftp.mydomain.org"));
    _ui.ftpUserEdit->setPlaceholderText(tr("john"));
    _ui.ftpPasswdEdit->setPlaceholderText(tr("secret"));
#endif
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
    return true;
}

// ======================================================================

CreateAnOwncloudPage::CreateAnOwncloudPage()
{
    _ui.setupUi(this);
    registerField(QLatin1String("createLocalOC"),  _ui.createLocalRadioBtn );
    registerField(QLatin1String("createOnDomain"), _ui.createPerFTPRadioBtn );
    registerField(QLatin1String("myOCDomain"),     _ui.myDomainEdit );

    connect( _ui.createLocalRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.createPerFTPRadioBtn, SIGNAL(clicked()), SIGNAL(completeChanged()));
    connect( _ui.myDomainEdit, SIGNAL(textChanged(QString)), SIGNAL(completeChanged()));

#if QT_VERSION >= 0x040700
    _ui.myDomainEdit->setPlaceholderText(tr("mydomain.org"));
#endif
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
    return (!dom.isEmpty() && dom.contains( QLatin1Char('.'))
            && dom.lastIndexOf(QLatin1Char('.')) < dom.length()-2 );
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

    setupCustomization();
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
    return true;
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

void OwncloudWizardResultPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->setText( QString::null );
    _ui.topLabel->hide();

    MirallConfigFile cfg;

    QVariant variant = cfg.customMedia( MirallConfigFile::oCSetupResultTop );
    setupCustomMedia( variant, _ui.topLabel );
}

// ======================================================================

/**
 * Folder wizard itself
 */

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
{
#ifdef OWNCLOUD_CLIENT
    setPage(Page_oCSetup,        new OwncloudSetupPage() );
#else
    setPage(Page_SelectType,     new OwncloudWizardSelectTypePage() );
    setPage(Page_OC_Credentials, new OwncloudCredentialsPage() );
#endif
    setPage(Page_Create_OC,      new CreateAnOwncloudPage() );
    setPage(Page_FTP,            new OwncloudFTPAccessPage() );
    setPage(Page_Install,        new OwncloudWizardResultPage() );

#ifdef Q_WS_MAC
    setWizardStyle( QWizard::ModernStyle );
#endif
    setField(QLatin1String("connectMyOC"), true);

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));

}

QString OwncloudWizard::ocUrl() const
{
    QString url = field("OCUrl").toString();

    if( field("secureConnect").toBool() ) {
        url.prepend(QLatin1String("https://"));
    } else {
        url.prepend(QLatin1String("http://"));
    }
    return url;
}

void OwncloudWizard::slotCurrentPageChanged( int id )
{
  qDebug() << "Current Wizard page changed to " << id;
  qDebug() << "Page_install is " << Page_Install;

  if( id == Page_FTP ) {
    // preset the ftp url field
    CreateAnOwncloudPage *p = static_cast<CreateAnOwncloudPage*> (page( Page_Create_OC ));
    QString domain = p->domain();
    if( domain.startsWith( QLatin1String("http://") )) {
      domain = domain.right( domain.length()-7 );
    }
    if( domain.startsWith( QLatin1String("https://") )) {
      domain = domain.right( domain.length()-8 );
    }

    QString host = QLatin1String("ftp.") +domain;
    OwncloudFTPAccessPage *p1 = static_cast<OwncloudFTPAccessPage*> (page( Page_FTP ));
    p1->setFTPUrl( host );
  }
  if( id == Page_Install ) {
    appendToResultWidget( QString::null );
    showOCUrlLabel( false );
    if( field(QLatin1String("connectMyOC")).toBool() ) {
      // check the url and connect.
      _oCUrl = ocUrl();
      emit connectToOCUrl( _oCUrl);
    } else if( field(QLatin1String("createLocalOC")).toBool() ) {
      qDebug() << "Connect to local!";
      emit installOCLocalhost();
    } else if( field(QLatin1String("createNewOC")).toBool() ) {
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
#ifdef OWNCLOUD_CLIENT
  OwncloudSetupPage *p = static_cast<OwncloudSetupPage*>(page(Page_oCSetup));
#else
  OwncloudWizardSelectTypePage *p = static_cast<OwncloudWizardSelectTypePage*>(page( Page_SelectType ));
#endif
  if( p )
      p->setOCUrl( url );

}

} // end namespace
