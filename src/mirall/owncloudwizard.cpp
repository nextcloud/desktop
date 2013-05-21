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
#include <QProgressIndicator.h>

#include <QtCore>
#include <QtGui>

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

    Theme *theme = Theme::instance();
    setTitle( tr("<font color=\"%1\" size=\"5\">Connect to your %2 Server</font>")
              .arg(theme->wizardHeaderTitleColor().name()).arg( theme->appNameGUI()));
    setSubTitle( tr("<font color=\"%1\">Enter user credentials to access your %2</font>")
                 .arg(theme->wizardHeaderTitleColor().name()).arg(theme->appNameGUI()));

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(handleNewOcUrl(QString)));

    registerField( QLatin1String("OCUrl"),    _ui.leUrl );
    registerField( QLatin1String("OCUser"),   _ui.leUsername );
    registerField( QLatin1String("OCPasswd"), _ui.lePassword);
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
    delete _progressIndi;
}

void OwncloudSetupPage::slotToggleAdvanced(int state)
{
    _ui.advancedBox->setVisible( state == Qt::Checked );
    wizard()->resize(wizard()->sizeHint());
}

void OwncloudSetupPage::slotChangedSelective(QAbstractButton* button)
{
    if( button = _ui.pbBoxMode ) {
        // box mode - sync the entire oC
    } else {
        // content mode, select folder list.
    }
}

void OwncloudSetupPage::setOCUser( const QString & user )
{
    if( _ui.leUsername->text().isEmpty() ) {
        _ui.leUsername->setText(user);
    }
}

void OwncloudSetupPage::setServerUrl( const QString& newUrl )
{
    QString url( newUrl );
    if( url.isEmpty() ) {
        _ui.leUrl->clear();
        return;
    }

    _ui.leUrl->setText( url );
}

void OwncloudSetupPage::setupCustomization()
{
    // set defaults for the customize labels.

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
        setServerUrl( fixUrl );
        _ui.leUrl->setEnabled( false );
        _ui.leUrl->hide();
    }
}

// slot hit from textChanged of the url entry field.
void OwncloudSetupPage::handleNewOcUrl(const QString& ocUrl)
{
    QString url = ocUrl;
    int len = 0;
    bool visible = false;
#if 0
    if (url.startsWith(QLatin1String("https://"))) {
        _ui.urlLabel->setPixmap( QPixmap(":/mirall/resources/security-high.png"));
        _ui.urlLabel->setToolTip(tr("This url is secure. You can use it."));
        visible = true;
    }
    if (url.startsWith(QLatin1String("http://"))) {
        _ui.urlLabel->setPixmap( QPixmap(":/mirall/resources/security-low.png"));
        _ui.urlLabel->setToolTip(tr("This url is NOT secure. You should not use it."));
        visible = true;
    }
#endif

}

bool OwncloudSetupPage::isComplete() const
{
    if( _ui.leUrl->text().isEmpty() ) return false;
    if( _checking ) return false;

    return !( _ui.lePassword->text().isEmpty() || _ui.leUsername->text().isEmpty() );
}

void OwncloudSetupPage::initializePage()
{
    _connected = false;
    _checking  = false;
}

int OwncloudSetupPage::nextId() const
{
  return OwncloudWizard::Page_Result;
}

QString OwncloudSetupPage::url() const
{
    QString url = _ui.leUrl->text().simplified();
    return url;
}

void OwncloudSetupPage::setConnected( bool comp )
{
    _connected = comp;
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

bool OwncloudSetupPage::validatePage()
{
    bool re = false;

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

void OwncloudSetupPage::setErrorString( const QString& err )
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _checking = false;
    emit completeChanged();
    stopSpinner();
}

void OwncloudSetupPage::stopSpinner()
{
    // _ui.addressLayout->removeWidget( _progressIndi );

    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

OwncloudSetupPage::SyncMode OwncloudSetupPage::syncMode()
{
    if( _selectiveSyncButtons->checkedButton() &&
            _selectiveSyncButtons->checkedButton() == _ui.pbSelectiveMode ) {
        return SelectiveMode;
    }
    return BoxMode;
}

void OwncloudSetupPage::setFolderNames( const QString& localFolder, const QString& remoteFolder )
{
    _ui.pbSelectLocalFolder->setText(localFolder);
    if( !remoteFolder.isEmpty() )
        _remoteFolder = remoteFolder;

    QString t;
    if( _remoteFolder.isEmpty() || _remoteFolder == QLatin1String("/") ) {
        t = tr("Your entire account will be synced to the local folder '%1'").arg(localFolder);
    } else {
        t = tr("ownCloud folder '%1' is synced to local folder '%2'").arg(_remoteFolder).arg(localFolder);
    }

    _ui.syncModeLabel->setText(t);
}

QString OwncloudSetupPage::selectedLocalFolder() const
{
    return _ui.pbSelectLocalFolder->text();
}

void OwncloudSetupPage::slotSelectFolder()
{

    QString dir = QFileDialog::getExistingDirectory(0, tr("Local Sync Folder"), QDir::homePath());
    if( !dir.isEmpty() ) {
        setFolderNames(dir);
    }
}

OwncloudSetupPage::SyncMode OwncloudWizard::syncMode()
{
    return _setupPage->syncMode();
    return OwncloudSetupPage::BoxMode;
}

// ======================================================================

OwncloudWizardResultPage::OwncloudWizardResultPage()
{
    _ui.setupUi(this);
    // no fields to register.

    Theme *theme = Theme::instance();
    setTitle( tr("<font color=\"%1\" size=\"5\">Everything set up!</font>")
              .arg(theme->wizardHeaderTitleColor().name()));
    setSubTitle( tr("<font color=\"%1\">Enter user credentials to access your %2</font>")
                 .arg(theme->wizardHeaderTitleColor().name()).arg(theme->appNameGUI()));

    _ui.pbOpenLocal->setText("Open local folder");
    _ui.pbOpenServer->setText(tr("Open %1").arg(Theme::instance()->appNameGUI()));

    _ui.pbOpenLocal->setIcon(QIcon(":/mirall/resources/folder-sync.png"));
    _ui.pbOpenLocal->setText(tr("Open Local Folder"));
    _ui.pbOpenLocal->setIconSize(QSize(48, 48));
    connect(_ui.pbOpenLocal, SIGNAL(clicked()), SLOT(slotOpenLocal()));

    _ui.pbOpenLocal->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

//    _ui.pbOpenServer->setIcon(QIcon(":/mirall/resources/owncloud_logo_blue.png"));
    _ui.pbOpenServer->setIcon(theme->applicationIcon().pixmap(48));
    _ui.pbOpenServer->setText(tr("Open %1").arg(theme->appNameGUI()));
    _ui.pbOpenServer->setIconSize(QSize(48, 48));
    _ui.pbOpenServer->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    connect(_ui.pbOpenServer, SIGNAL(clicked()), SLOT(slotOpenServer()));
    setupCustomization();
}

OwncloudWizardResultPage::~OwncloudWizardResultPage()
{
}

void OwncloudWizardResultPage::setComplete(bool complete)
{
    _complete = complete;
    emit completeChanged();
}

bool OwncloudWizardResultPage::isComplete() const
{
    return _complete;
}

void OwncloudWizardResultPage::setFolderNames( const QString& localFolder, const QString& remoteFolder )
{
    _localFolder = localFolder;
    QString text;
    if( remoteFolder == QLatin1String("/") ||
            remoteFolder.isEmpty() ) {
        text = tr("Your entire account is synced to the local folder <i>%1</i>").arg(localFolder);
    } else {
        text = tr("ownCloud folder <i>%1</i> is synced to local folder <i>%2</i>").arg(remoteFolder).arg(localFolder);
    }
    _ui.localFolderLabel->setText( text );
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
    _setupPage  = new OwncloudSetupPage;
    _resultPage = new OwncloudWizardResultPage;
    setPage(Page_oCSetup, _setupPage  );
    setPage(Page_Result,  _resultPage );

    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle( QWizard::ModernStyle );

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));

    connect( _setupPage, SIGNAL(connectToOCUrl(QString)), SIGNAL(connectToOCUrl(QString)));


    Theme *theme = Theme::instance();
    setWizardStyle(QWizard::ModernStyle);
    setPixmap( QWizard::BannerPixmap, theme->wizardHeaderBanner() );
    setPixmap( QWizard::LogoPixmap, theme->wizardHeaderLogo() );
    setOption( QWizard::NoBackButtonOnStartPage );
    setOption( QWizard::NoBackButtonOnLastPage );
    setOption( QWizard::NoCancelButton );
    setTitleFormat(Qt::RichText);
    setSubTitleFormat(Qt::RichText);
}

QString OwncloudWizard::ocUrl() const
{
    QString url = field("OCUrl").toString().simplified();
    return url;
}

void OwncloudWizard::enableFinishOnResultWidget(bool enable)
{
    _resultPage->setComplete(enable);
}

void OwncloudWizard::setFolderNames( const QString& localFolder, const QString& remoteFolder )
{
    _setupPage->setFolderNames( localFolder, remoteFolder );
    _resultPage->setFolderNames( localFolder, remoteFolder );
}

void OwncloudWizard::showConnectInfo( const QString& msg )
{
    if( _setupPage ) {
        _setupPage->setErrorString( msg );
    }
}

void OwncloudWizard::successfullyConnected(bool enable)
{
    _setupPage->setConnected( enable );

    if( enable ) {
        next();
    }
}

void OwncloudWizard::slotCurrentPageChanged( int id )
{
    qDebug() << "Current Wizard page changed to " << id;

    if( id == Page_oCSetup ) {
        setButtonText( QWizard::NextButton, tr("Connect...") );
        emit clearPendingRequests();
        _setupPage->initializePage();

    }

    if( id == Page_Result ) {
        appendToConfigurationLog( QString::null );
    }
}

void OwncloudWizard::displayError( const QString& msg )
{
    _setupPage->setErrorString( msg );
}

void OwncloudWizard::appendToConfigurationLog( const QString& msg, LogType type )
{
    _setupLog << msg;
    qDebug() << "Setup-Log: " << msg;
}

void OwncloudWizard::setOCUrl( const QString& url )
{
  _oCUrl = url;
  _setupPage->setServerUrl( url );
}

void OwncloudWizard::setOCUser( const QString& user )
{
  _oCUser = user;
  _setupPage->setOCUser( user );
}

void OwncloudWizardResultPage::slotOpenLocal()
{
    QDesktopServices::openUrl(QUrl(_localFolder));
}

void OwncloudWizardResultPage::slotOpenServer()
{
    QUrl url = field("OCUrl").toUrl();
    qDebug() << Q_FUNC_INFO << url;
    QDesktopServices::openUrl(url);
}

} // end namespace
