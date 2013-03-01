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
#include "mirall/theme.h"

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
    if( !label ) return;

    QPixmap pix = variant.value<QPixmap>();
    if( !pix.isNull() ) {
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

    setTitle(tr("Create Connection to %1").arg(Theme::instance()->appNameGUI()));

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(handleNewOcUrl(QString)));

    registerField( QLatin1String("OCUrl"), _ui.leUrl );
    registerField( QLatin1String("OCUser"),   _ui.leUsername );
    registerField( QLatin1String("OCPasswd"), _ui.lePassword);
    registerField( QLatin1String("connectMyOC"), _ui.cbConnectOC );
    registerField( QLatin1String("secureConnect"), _ui.cbSecureConnect );
    registerField( QLatin1String("PwdNoLocalStore"), _ui.cbNoPasswordStore );

    _ui.cbSecureConnect->setEnabled(QSslSocket::supportsSsl());

    connect( _ui.lePassword, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    connect( _ui.leUsername, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    connect( _ui.cbAdvanced, SIGNAL(stateChanged (int)), SLOT(slotToggleAdvanced(int)));
    connect( _ui.pbSelectLocalFolder, SIGNAL(clicked()), SLOT(slotSelectFolder()));
    _ui.errorLabel->setVisible(true);
    _ui.advancedBox->setVisible(false);

    _progressIndi = new QProgressIndicator;
    _ui.resultLayout->addWidget( _progressIndi );
    _progressIndi->setVisible(false);

    // Error label
    QString style = QLatin1String("border: 1px solid #eed3d7; border-radius: 5px; padding: 3px;"
                                  "background-color: #f2dede; color: #b94a48;");


    _ui.errorLabel->setStyleSheet( style );
    _ui.errorLabel->setWordWrap(true);
    _ui.errorLabel->setVisible(false);
    setTitle( tr("<font color=\"#ffffff\" size=\"5\">Connect to your %1 Server</font>").arg( Theme::instance()->appNameGUI()));
    setSubTitle( tr("<font color=\"#1d2d42\">Enter user credentials to access your %1</font>").arg(Theme::instance()->appNameGUI()));

    // ButtonGroup for
    _selectiveSyncButtons = new QButtonGroup;
    _selectiveSyncButtons->addButton( _ui.pbBoxMode );
    _selectiveSyncButtons->addButton( _ui.pbSelectiveMode );
    connect( _selectiveSyncButtons, SIGNAL(buttonClicked (QAbstractButton*)),
             SLOT(slotChangedSelective(QAbstractButton*)));

    _ui.selectiveSyncLabel->setVisible(false);
    _ui.pbBoxMode->setVisible(false);
    _ui.pbSelectiveMode->setVisible(false);

    _checking = false;

    setupCustomization();
}


OwncloudSetupPage::~OwncloudSetupPage()
{
}

void OwncloudSetupPage::setOCUser( const QString & user )
{
    if( _ui.leUsername->text().isEmpty() ) {
        _ui.leUsername->setText(user);
    }
}

void  OwncloudSetupPage::setAllowPasswordStorage( bool allow )
{
    _ui.cbNoPasswordStore->setChecked( ! allow );
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

    // _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia( Theme::oCSetupTop );
    if( variant.isNull() ) {
        _ui.topLabel->setOpenExternalLinks(true);
        _ui.topLabel->setText("If you don't have an ownCloud server yet, see <a href=\"https://owncloud.com\">owncloud.com</a> for more info.");
    } else {
        setupCustomMedia( variant, _ui.topLabel );
    }

    variant = theme->customMedia( Theme::oCSetupBottom );
    setupCustomMedia( variant, _ui.bottomLabel );

    QString fixUrl = theme->overrideServerUrl();
    if( !fixUrl.isEmpty() ) {
        setOCUrl( fixUrl );
        _ui.leUrl->setEnabled( false );
        _ui.cbSecureConnect->hide();
        _ui.leUrl->hide();
    }
}

void OwncloudSetupPage::handleNewOcUrl(const QString& ocUrl)
{
    QString url = ocUrl;
    int len = 0;
    if (url.startsWith(QLatin1String("https://"))) {
        _ui.cbSecureConnect->setChecked(true);
        len = 8;
    }
    if (url.startsWith(QLatin1String("http://"))) {
        _ui.cbSecureConnect->setChecked(false);
        len = 7;
    }
    if( len ) {
        int pos = _ui.leUrl->cursorPosition();
        url.remove(0, len);
        _ui.leUrl->setText(url);
        _ui.leUrl->setCursorPosition(qMax(0, pos-len));

    }
}

bool OwncloudSetupPage::isComplete() const
{
    if( _ui.leUrl->text().isEmpty() ) return false;
    if( _checking ) return false;

    if( _ui.cbNoPasswordStore->checkState() == Qt::Checked ) {
        return !(_ui.leUsername->text().isEmpty());
    }
    return !(_ui.leUsername->text().isEmpty() || _ui.lePassword->text().isEmpty() );
}

void OwncloudSetupPage::initializePage()
{
}

int OwncloudSetupPage::nextId() const
{
  return OwncloudWizard::Page_Result;
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

    if( ! _connected) {
        setErrorString(QString::null);
        _checking = true;
        _progressIndi->setVisible(true);
        _progressIndi->startAnimation();
        emit completeChanged();

        emit connectToOCUrl( url() );
        return false;
    } else {
        // connecting is running
        stopSpinner();
        _checking = false;
        emit completeChanged();
        return true;
    }
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
    _checking = false;
    emit completeChanged();
    stopSpinner();
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
    _ui.pbSelectLocalFolder->setText(folder);

    QString t = tr("Your entire account will be synced to the local folder %1").arg(folder);
    _ui.syncModeLabel->setText(t);
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

    QString dir = QFileDialog::getExistingDirectory(0, tr("Local Sync Folder"), QDir::homePath());
    if( !dir.isEmpty() ) {
        setLocalFolder(dir);
    }
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

    _ui.pbOpenLocal->setText("Open local folder");

    _ui.pbOpenServer->setText(tr("Open %1").arg(Theme::instance()->appNameGUI()));

    setTitle( tr("<font color=\"#ffffff\" size=\"5\">Everything set up!</font>").arg( Theme::instance()->appNameGUI()));
    setSubTitle( tr("<font color=\"#1d2d42\">Enter user credentials to access your %1</font>").arg(Theme::instance()->appNameGUI()));

    _ui.pbOpenLocal->setIcon(QIcon(":/mirall/resources/folder-sync.png"));
    _ui.pbOpenLocal->setText(tr("Open Local Folder"));
    _ui.pbOpenLocal->setIconSize(QSize(48, 48));

    _ui.pbOpenLocal->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    _ui.pbOpenServer->setIcon(QIcon(":/mirall/resources/mirall-48.png"));
    _ui.pbOpenServer->setText(tr("Open Server"));
    _ui.pbOpenServer->setIconSize(QSize(48, 48));
    _ui.pbOpenServer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setupCustomization();
}

OwncloudWizardResultPage::~OwncloudWizardResultPage()
{
}

void OwncloudWizardResultPage::setOwncloudUrl( const QString& url )
{
    _url = url;
}

void OwncloudWizardResultPage::setLocalFolder( const QString& folder )
{
    _localFolder = folder;
    _ui.localFolderLabel->setText(tr("Your entire account is synced to the local folder %1").arg(folder));
}

void OwncloudWizardResultPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->setText( QString::null );
    _ui.topLabel->hide();

    QVariant variant = Theme::instance()->customMedia( Theme::oCSetupResultTop );
    setupCustomMedia( variant, _ui.topLabel );
}

// ======================================================================

/**
 * Folder wizard itself
 */

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_oCSetup,    new OwncloudSetupPage() );
    setPage(Page_Result,    new OwncloudWizardResultPage() );
    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle( QWizard::ModernStyle );

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));

    OwncloudSetupPage *p = static_cast<OwncloudSetupPage*>(page(Page_oCSetup));
    connect( p, SIGNAL(connectToOCUrl(QString)), SIGNAL(connectToOCUrl(QString)));

    QPixmap pix(QSize(540, 78));
    pix.fill(QColor("#1d2d42"));
    setPixmap( QWizard::BannerPixmap, pix );

    QPixmap logo( ":/mirall/resources/owncloud_logo.png");
    setPixmap( QWizard::LogoPixmap, logo );
    setWizardStyle(QWizard::ModernStyle);
    setOption( QWizard::NoBackButtonOnStartPage );
    setOption( QWizard::NoCancelButton );
    setTitleFormat(Qt::RichText);
    setSubTitleFormat(Qt::RichText);

}

QString OwncloudWizard::ocUrl() const
{
    QString url = field("OCUrl").toString().simplified();
    if( field("secureConnect").toBool() ) {
        url.prepend(QLatin1String("https://"));
    } else {
        url.prepend(QLatin1String("http://"));
    }
    return url;
}

void OwncloudWizard::enableFinishOnResultWidget(bool enable)
{
    OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Install ));
    p->setComplete(enable);
}

void OwncloudWizard::slotCurrentPageChanged( int id )
{
  qDebug() << "Current Wizard page changed to " << id;

    QString host = QLatin1String("ftp.") +domain;
    OwncloudFTPAccessPage *p1 = static_cast<OwncloudFTPAccessPage*> (page( Page_FTP ));
    p1->setFTPUrl( host );
  }

  if( id == Page_Result ) {
    appendToResultWidget( QString::null );
    showOCUrlLabel( false );
    OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Result ));
    if( p ) p->setLocalFolder( selectedLocalFolder() );
  }
}

void OwncloudWizard::showOCUrlLabel( bool show )
{
  OwncloudWizardResultPage *p = static_cast<OwncloudWizardResultPage*> (page( Page_Result ));
}

void OwncloudWizard::appendToResultWidget( const QString& msg, LogType type )
{
  OwncloudWizardResultPage *r = static_cast<OwncloudWizardResultPage*> (page( Page_Result ));
  qDebug() << "XXXXXXXXXXXXX " << msg;
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

void OwncloudWizard::setOCUser( const QString& user )
{
  _oCUser = user;
#ifdef OWNCLOUD_CLIENT
  OwncloudSetupPage *p = static_cast<OwncloudSetupPage*>(page(Page_oCSetup));
  if( p )
      p->setOCUser( user );
#else
  OwncloudWizardSelectTypePage *p = static_cast<OwncloudWizardSelectTypePage*>(page( Page_SelectType ));
#endif
}

void OwncloudWizard::setAllowPasswordStorage( bool allow )
{
#ifdef OWNCLOUD_CLIENT
  OwncloudSetupPage *p = static_cast<OwncloudSetupPage*>(page(Page_oCSetup));
  if( p )
      p->setAllowPasswordStorage( allow );
#endif
}

} // end namespace
