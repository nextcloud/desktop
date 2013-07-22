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

#include "mirall/wizard/owncloudwizard.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "mirall/wizard/owncloudsetuppage.h"
#include "mirall/wizard/owncloudhttpcredspage.h"
#include "mirall/wizard/owncloudwizardresultpage.h"

#include "QProgressIndicator.h"

#include <QtCore>
#include <QtGui>

#include <stdlib.h>

namespace Mirall
{

WizardCommon::SyncMode OwncloudWizard::syncMode()
{
    return _setupPage->syncMode();
    return WizardCommon::BoxMode;
}

void OwncloudWizard::setMultipleFoldersExist(bool exist)
{
    _setupPage->setMultipleFoldersExist(exist);
}

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent),
      _configExists(false)
{
    _setupPage  = new OwncloudSetupPage;
    _httpCredsPage  = new OwncloudHttpCredsPage;
    _resultPage = new OwncloudWizardResultPage;
    setPage(WizardCommon::Page_oCSetup, _setupPage  );
    setPage(WizardCommon::Page_HttpCreds, _httpCredsPage);
    setPage(WizardCommon::Page_Result,  _resultPage );

    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle( QWizard::ModernStyle );

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));
    connect( _setupPage, SIGNAL(determineAuthType(QString)), SIGNAL(determineAuthType(QString)));
    connect( _httpCredsPage, SIGNAL(connectToOCUrl(QString)), SIGNAL(connectToOCUrl(QString)));


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
    _httpCredsPage->setConnected( enable );

    if( enable ) {
        next();
    }
}

void OwncloudWizard::setAuthType(WizardCommon::AuthType type)
{
  _setupPage->setAuthType(type);
  next();
}

void OwncloudWizard::slotCurrentPageChanged( int id )
{
    qDebug() << "Current Wizard page changed to " << id;

    if( id == WizardCommon::Page_oCSetup ) {
        setButtonText( QWizard::NextButton, tr("Connect...") );
        emit clearPendingRequests();
        _setupPage->initializePage();

    }

    if( id == WizardCommon::Page_Result ) {
        appendToConfigurationLog( QString::null );
    }
}

void OwncloudWizard::displayError( const QString& msg )
{
    if (currentId() == WizardCommon::Page_oCSetup) {
        _setupPage->setErrorString( msg );
    } else {
        _httpCredsPage->setErrorString(msg);
    }
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
  _httpCredsPage->setOCUser( user );
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

} // end namespace
