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

#include "QProgressIndicator.h"

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
    setTitle( tr("<font color=\"%1\" size=\"5\">Connect to %2</font>")
              .arg(theme->wizardHeaderTitleColor().name()).arg( theme->appNameGUI()));
    setSubTitle( tr("<font color=\"%1\">Enter user credentials</font>")
                 .arg(theme->wizardHeaderTitleColor().name()));

    registerField( QLatin1String("OCUrl"),    _ui.leUrl );
    registerField( QLatin1String("OCUser"),   _ui.leUsername );
    registerField( QLatin1String("OCPasswd"), _ui.lePassword);
    registerField( QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    _ui.errorLabel->setVisible(true);
    _ui.advancedBox->setVisible(false);

    _progressIndi = new QProgressIndicator;
    _ui.resultLayout->addWidget( _progressIndi );
    _progressIndi->setVisible(false);
    _ui.resultLayout->setEnabled(false);

    // Error label
    QString style = QLatin1String("border: 1px solid #eed3d7; border-radius: 5px; padding: 3px;"
                                  "background-color: #f2dede; color: #b94a48;");


    _ui.errorLabel->setStyleSheet( style );
    _ui.errorLabel->setWordWrap(true);
    _ui.errorLabel->setVisible(false);

    _checking = false;

    setupCustomization();

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(slotUrlChanged(QString)));
    connect( _ui.leUsername, SIGNAL(textChanged(QString)), this, SLOT(slotUserChanged(QString)));

    connect( _ui.lePassword, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    connect( _ui.leUsername, SIGNAL(textChanged(QString)), this, SIGNAL(completeChanged()));
    connect( _ui.cbAdvanced, SIGNAL(stateChanged (int)), SLOT(slotToggleAdvanced(int)));
    connect( _ui.pbSelectLocalFolder, SIGNAL(clicked()), SLOT(slotSelectFolder()));
}

OwncloudSetupPage::~OwncloudSetupPage()
{
    delete _progressIndi;
}

void OwncloudSetupPage::slotToggleAdvanced(int state)
{
    _ui.advancedBox->setVisible( state == Qt::Checked );
    slotHandleUserInput();
    QSize size = wizard()->sizeHint();
    // need to substract header for some reason
    size -= QSize(0, 63);

    wizard()->setMinimumSize(size);
    wizard()->resize(size);
}

void OwncloudSetupPage::setOCUser( const QString & user )
{
    _ocUser = user;
    _ui.leUsername->setText(user);
}

void OwncloudSetupPage::setServerUrl( const QString& newUrl )
{
    _oCUrl = newUrl;
    if( _oCUrl.isEmpty() ) {
        _ui.leUrl->clear();
        return;
    }

    _ui.leUrl->setText( _oCUrl );
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
        _ui.label_2->hide();
        setServerUrl( fixUrl );
        _ui.leUrl->setEnabled( false );
        _ui.leUrl->hide();
    }
}

void OwncloudSetupPage::slotUserChanged(const QString& user )
{
    slotHandleUserInput();
}

// slot hit from textChanged of the url entry field.
void OwncloudSetupPage::slotUrlChanged(const QString& ocUrl)
{
    slotHandleUserInput();

#if 0
    QString url = ocUrl;
    bool visible = false;

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
    _multipleFoldersExist = false;

    if( _configExists ) {
        _ui.lePassword->setFocus();
    } else {
        _ui.leUrl->setFocus();
    }
}

bool OwncloudSetupPage::urlHasChanged()
{
    bool change = false;
    const QChar slash('/');

    QUrl currentUrl( url() );
    QUrl initialUrl( _oCUrl );

    QString currentPath = currentUrl.path();
    QString initialPath = initialUrl.path();

    // add a trailing slash.
    if( ! currentPath.endsWith( slash )) currentPath += slash;
    if( ! initialPath.endsWith( slash )) initialPath += slash;

    if( currentUrl.host() != initialUrl.host() ||
            currentPath != initialPath ) {
        change = true;
    }

    if( !change) { // no change yet, check the user.
        QString user = _ui.leUsername->text().simplified();
        if( user != _ocUser ) change = true;
    }

    return change;
}

// Called if the user changes the user- or url field. Adjust the texts and
// evtl. warnings on the dialog.
void OwncloudSetupPage::slotHandleUserInput()
{
    // if the url has not changed, return.
    if( ! urlHasChanged() ) {
        // disable the advanced button as nothing has changed.
        _ui.cbAdvanced->setEnabled(false);
        _ui.advancedBox->setEnabled(false);
    } else {
        // Enable advanced stuff for new connection configuration.
        _ui.cbAdvanced->setEnabled(true);
        _ui.advancedBox->setEnabled(true);
    }

    const QString locFolder = localFolder();

    // check if the local folder exists. If so, and if its not empty, show a warning.
    QDir dir( locFolder );
    QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);

    QString t;

    if( !urlHasChanged() && _configExists ) {
        // This is the password change mode: No change to the url and a config
        // to an ownCloud exists.
        t = tr("Change the Password for your configured account.");
    } else {
        // Complete new setup.
        _ui.pbSelectLocalFolder->setText(locFolder);

        if( _remoteFolder.isEmpty() || _remoteFolder == QLatin1String("/") ) {
            t = tr("Your entire account will be synced to the local folder '%1'.")
                    .arg(QDir::toNativeSeparators(locFolder));
        } else {
            t = tr("%1 folder '%2' is synced to local folder '%3'")
                    .arg(Theme::instance()->appName()).arg(_remoteFolder)
                    .arg(QDir::toNativeSeparators(locFolder));
        }

        if ( _multipleFoldersExist ) {
            t += tr("<p><small><strong>Warning:</strong> You currently have multiple folders "
                    "configured. If you continue with the current settings, the folder configurations "
                    "will be discarded and a single root folder sync will be created!</small></p>");
        }

        if( entries.count() > 0) {
            // the directory is not empty
            if (!_ui.cbAdvanced->isChecked()) {
                t += tr("<p><small><strong>Warning:</strong> The local directory is not empty. "
                        "Pick a resolution in the advanced settings!</small></p>");
            }
            _ui.resolutionWidget->setVisible(true);
        } else {
            // the dir is empty, which means that there is no problem.
            _ui.resolutionWidget->setVisible(false);
        }
    }

    _ui.syncModeLabel->setText(t);
    _ui.syncModeLabel->setFixedHeight(_ui.syncModeLabel->sizeHint().height());
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

QString OwncloudSetupPage::localFolder() const
{
    QString folder = wizard()->property("localFolder").toString();
    return folder;
}

void OwncloudSetupPage::setConnected( bool comp )
{
    _connected = comp;
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

bool OwncloudSetupPage::validatePage()
{
    bool re = false;

    if( ! _connected) {
        setErrorString(QString::null);
        _checking = true;
        _ui.resultLayout->setEnabled(true);
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
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

OwncloudSetupPage::SyncMode OwncloudSetupPage::syncMode()
{
    return BoxMode;
}

void OwncloudSetupPage::setRemoteFolder( const QString& remoteFolder )
{
    if( !remoteFolder.isEmpty() ) {
        _remoteFolder = remoteFolder;
    }
}

void OwncloudSetupPage::setMultipleFoldersExist(bool exist)
{
    _multipleFoldersExist = exist;
}

void OwncloudSetupPage::slotSelectFolder()
{

    QString dir = QFileDialog::getExistingDirectory(0, tr("Local Sync Folder"), QDir::homePath());
    if( !dir.isEmpty() ) {
        _ui.pbSelectLocalFolder->setText(dir);
        wizard()->setProperty("localFolder", dir);
        slotHandleUserInput();
    }
}

OwncloudSetupPage::SyncMode OwncloudWizard::syncMode()
{
    return _setupPage->syncMode();
    return OwncloudSetupPage::BoxMode;
}

void OwncloudWizard::setMultipleFoldersExist(bool exist)
{
    _setupPage->setMultipleFoldersExist(exist);
}

void OwncloudSetupPage::setConfigExists(  bool config )
{
    _configExists = config;
    setSubTitle( tr("<font color=\"%1\">Change your user credentials</font>")
                 .arg(Theme::instance()->wizardHeaderTitleColor().name()));
}

// ======================================================================

OwncloudWizardResultPage::OwncloudWizardResultPage()
{
    _ui.setupUi(this);
    // no fields to register.

    Theme *theme = Theme::instance();
    setTitle( tr("<font color=\"%1\" size=\"5\">Everything set up!</font>")
              .arg(theme->wizardHeaderTitleColor().name()));
    // required to show header in QWizard's modern style
    setSubTitle( QLatin1String(" ") );

    _ui.pbOpenLocal->setText("Open local folder");
    _ui.pbOpenServer->setText(tr("Open %1").arg(Theme::instance()->appNameGUI()));

    _ui.pbOpenLocal->setIcon(QIcon(":/mirall/resources/folder-sync.png"));
    _ui.pbOpenLocal->setText(tr("Open Local Folder"));
    _ui.pbOpenLocal->setIconSize(QSize(48, 48));
    connect(_ui.pbOpenLocal, SIGNAL(clicked()), SLOT(slotOpenLocal()));

    _ui.pbOpenLocal->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);

    QIcon appIcon = theme->applicationIcon();
    _ui.pbOpenServer->setIcon(appIcon.pixmap(48));
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

void OwncloudWizardResultPage::initializePage()
{
    const QString localFolder = wizard()->property("localFolder").toString();
    QString text;
    if( _remoteFolder == QLatin1String("/") || _remoteFolder.isEmpty() ) {
        text = tr("Your entire account is synced to the local folder <i>%1</i>")
                .arg(QDir::toNativeSeparators(localFolder));
    } else {
        text = tr("ownCloud folder <i>%1</i> is synced to local folder <i>%2</i>")
                .arg(_remoteFolder).arg(QDir::toNativeSeparators(localFolder));
    }
    _ui.localFolderLabel->setText( text );

}

void OwncloudWizardResultPage::setRemoteFolder(const QString &remoteFolder)
{
    _remoteFolder = remoteFolder;
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
    : QWizard(parent),
      _configExists(false)
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

QString OwncloudWizard::localFolder() const
{
    return(_setupPage->localFolder());
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

void OwncloudWizard::setRemoteFolder( const QString& remoteFolder )
{
    _setupPage->setRemoteFolder( remoteFolder );
    _resultPage->setRemoteFolder( remoteFolder );
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
  _setupPage->setServerUrl( url );
}

void OwncloudWizard::setOCUser( const QString& user )
{
  _oCUser = user;
  _setupPage->setOCUser( user );
}

void OwncloudWizard::setConfigExists( bool config )
{
    _configExists = config;
    _setupPage->setConfigExists( config );
}

bool OwncloudWizard::configExists()
{
    return _configExists;
}

void OwncloudWizardResultPage::slotOpenLocal()
{
    const QString localFolder = wizard()->property("localFolder").toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(localFolder));
}

void OwncloudWizardResultPage::slotOpenServer()
{
    QUrl url = field("OCUrl").toUrl();
    qDebug() << Q_FUNC_INFO << url;
    QDesktopServices::openUrl(url);
}


} // end namespace
