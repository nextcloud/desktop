/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudadvancedsetuppage.h"
#include "mirall/theme.h"

namespace Mirall
{

OwncloudAdvancedSetupPage::OwncloudAdvancedSetupPage()
  : QWizardPage(),
    _ui(),
    _checking(false),
    _created(false),
    _configExists(false),
    _multipleFoldersExist(false),
    _remoteFolder(),
    _progressIndi(new QProgressIndicator (this))
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(theme->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Setup local folder options")));

    registerField( QLatin1String("OCSyncFromScratch"), _ui.cbSyncFromScratch);

    _ui.resultLayout->addWidget( _progressIndi );
    stopSpinner();
    setupCustomization();

    connect( _ui.pbSelectLocalFolder, SIGNAL(clicked()), SLOT(slotSelectFolder()));
}

void OwncloudAdvancedSetupPage::setupCustomization()
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
}

bool OwncloudAdvancedSetupPage::isComplete() const
{
    return !_checking;
}

void OwncloudAdvancedSetupPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    _checking  = false;
    _multipleFoldersExist = false;

    // call to init label
    updateStatus();

    // TODO: focus
    _ui.pbSelectLocalFolder->setFocus();
}

// Called if the user changes the user- or url field. Adjust the texts and
// evtl. warnings on the dialog.
void OwncloudAdvancedSetupPage::updateStatus()
{
    const QString locFolder = localFolder();
    // check if the local folder exists. If so, and if its not empty, show a warning.
    QString t;

    // TODO: Maybe handle "modify" mode differently.
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

    if(QDir(locFolder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).count() > 0) {
        t += tr("<p><small><strong>Warning:</strong> The local directory is not empty.</small></p>");
    }

    _ui.syncModeLabel->setText(t);
    _ui.syncModeLabel->setFixedHeight(_ui.syncModeLabel->sizeHint().height());
}

void OwncloudAdvancedSetupPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudAdvancedSetupPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

int OwncloudAdvancedSetupPage::nextId() const
{
    return WizardCommon::Page_Result;
}

QString OwncloudAdvancedSetupPage::localFolder() const
{
    QString folder = wizard()->property("localFolder").toString();
    return folder;
}

bool OwncloudAdvancedSetupPage::validatePage()
{
    if(!_created) {
        setErrorString(QString::null);
        _checking = true;
        startSpinner();
        emit completeChanged();

        emit createLocalAndRemoteFolders(localFolder(), _remoteFolder);
        return false;
    } else {
        // connecting is running
        _checking = false;
        emit completeChanged();
        stopSpinner();
        return true;
    }
}

void OwncloudAdvancedSetupPage::setErrorString( const QString& err )
{
    if( err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    _checking = false;
    emit completeChanged();
}

void OwncloudAdvancedSetupPage::directoriesCreated()
{
    _checking = false;
    _created = true;
    stopSpinner();
    emit completeChanged();
}

void OwncloudAdvancedSetupPage::setRemoteFolder( const QString& remoteFolder )
{
    if( !remoteFolder.isEmpty() ) {
        _remoteFolder = remoteFolder;
    }
}

void OwncloudAdvancedSetupPage::setMultipleFoldersExist(bool exist)
{
    _multipleFoldersExist = exist;
}

void OwncloudAdvancedSetupPage::slotSelectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(0, tr("Local Sync Folder"), QDir::homePath());
    if( !dir.isEmpty() ) {
        _ui.pbSelectLocalFolder->setText(dir);
        wizard()->setProperty("localFolder", dir);
        updateStatus();
    }
}

void OwncloudAdvancedSetupPage::setConfigExists(  bool config )
{
    _configExists = config;

    if (config == true) {
        setSubTitle( tr("<font color=\"%1\">Change your user credentials</font>")
                     .arg(Theme::instance()->wizardHeaderTitleColor().name()));
    }
}

} // ns Mirall
