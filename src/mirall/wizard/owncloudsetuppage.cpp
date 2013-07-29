/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@kde.org>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include <QDir>
#include <QFileDialog>
#include <QUrl>

#include "QProgressIndicator.h"

#include "mirall/wizard/owncloudwizardcommon.h"
#include "mirall/wizard/owncloudsetuppage.h"
#include "mirall/theme.h"

namespace Mirall
{

OwncloudSetupPage::OwncloudSetupPage()
  : QWizardPage(),
    _ui(),
    _oCUrl(),
    _ocUser(),
    _authTypeKnown(false),
    _checking(false),
    _multipleFoldersExist(false),
    _authType(WizardCommon::HttpCreds),
    _progressIndi(new QProgressIndicator (this)),
    _selectiveSyncButtons(0),
    _remoteFolder()
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup ownCloud server")));

    registerField( QLatin1String("OCUrl*"), _ui.leUrl );
    registerField( QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    _ui.advancedBox->setVisible(false);

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();

    setupCustomization();

    connect(_ui.leUrl, SIGNAL(textChanged(QString)), SLOT(slotUrlChanged(QString)));
    connect( _ui.cbAdvanced, SIGNAL(stateChanged (int)), SLOT(slotToggleAdvanced(int)));
    connect( _ui.pbSelectLocalFolder, SIGNAL(clicked()), SLOT(slotSelectFolder()));
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
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia( Theme::oCSetupTop );
    if( !variant.isNull() ) {
        WizardCommon::setupCustomMedia( variant, _ui.topLabel );
    }

    variant = theme->customMedia( Theme::oCSetupBottom );
    WizardCommon::setupCustomMedia( variant, _ui.bottomLabel );

    QString fixUrl = theme->overrideServerUrl();
    if( !fixUrl.isEmpty() ) {
        _ui.label_2->hide();
        setServerUrl( fixUrl );
        _ui.leUrl->setEnabled( false );
        _ui.leUrl->hide();
    }
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
        _ui.urlLabel->setToolTip(tr("This url is NOT secure. You should not use it."));W
        visible = true;
    }
#endif
}

bool OwncloudSetupPage::isComplete() const
{
    return !_ui.leUrl->text().isEmpty() && !_checking;
}

void OwncloudSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _authTypeKnown = false;
    _checking  = false;
    _multipleFoldersExist = false;

    // call to init label
    slotHandleUserInput();

    _ui.leUrl->setFocus();
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
        currentUrl.port() != initialUrl.port() ||
            currentPath != initialPath ) {
        change = true;
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
        t = tr("Press Next to change the Password for your configured account.");
    } else {
        // Complete new setup.
        _ui.pbSelectLocalFolder->setText(QDir::toNativeSeparators(locFolder));

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
    if (_authType == WizardCommon::HttpCreds) {
        return WizardCommon::Page_HttpCreds;
    } else {
        return WizardCommon::Page_ShibbolethCreds;
    }
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

bool OwncloudSetupPage::validatePage()
{
    bool re = false;

    if( ! _authTypeKnown) {
        setErrorString(QString::null);
        _checking = true;
        startSpinner ();
        emit completeChanged();

        emit determineAuthType(url());
        return false;
    } else {
        // connecting is running
        stopSpinner();
        _checking = false;
        emit completeChanged();
        return true;
    }
}

void OwncloudSetupPage::setAuthType (WizardCommon::AuthType type)
{
  _authTypeKnown = true;
  _authType = type;
  stopSpinner();
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

void OwncloudSetupPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudSetupPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

WizardCommon::SyncMode OwncloudSetupPage::syncMode()
{
    return WizardCommon::BoxMode;
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

void OwncloudSetupPage::setConfigExists(  bool config )
{
    _configExists = config;

    if (config == true) {
        setSubTitle( tr("<font color=\"%1\">Change your user credentials</font>")
                     .arg(Theme::instance()->wizardHeaderTitleColor().name()));
    }
}

} // ns Mirall
