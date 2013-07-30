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

#include "wizard/owncloudwizard.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/theme.h"
#include "wizard/owncloudsetuppage.h"
#include "wizard/owncloudhttpcredspage.h"
#include "wizard/owncloudshibbolethcredspage.h"
#include "wizard/owncloudwizardresultpage.h"

#include "QProgressIndicator.h"

#include <QtCore>
#include <QtGui>

#include <stdlib.h>

namespace Mirall
{

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent),
      _setupPage(new OwncloudSetupPage),
      _httpCredsPage(new OwncloudHttpCredsPage),
      _shibbolethCredsPage(new OwncloudShibbolethCredsPage),
      _resultPage(new OwncloudWizardResultPage),
      _credentialsPage(0),
      _configFile(),
      _oCUser(),
      _setupLog(),
      _configExists(false)
{
    setPage(WizardCommon::Page_oCSetup, _setupPage  );
    setPage(WizardCommon::Page_HttpCreds, _httpCredsPage);
    setPage(WizardCommon::Page_ShibbolethCreds, _shibbolethCredsPage);
    setPage(WizardCommon::Page_Result,  _resultPage );

    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle( QWizard::ModernStyle );

    connect( this, SIGNAL(currentIdChanged(int)), SLOT(slotCurrentPageChanged(int)));
    connect( _setupPage, SIGNAL(determineAuthType(QString)), SIGNAL(determineAuthType(QString)));
    connect( _httpCredsPage, SIGNAL(connectToOCUrl(QString)), SIGNAL(connectToOCUrl(QString)));
    connect( _shibbolethCredsPage, SIGNAL(connectToOCUrl(QString)), SIGNAL(connectToOCUrl(QString)));

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

WizardCommon::SyncMode OwncloudWizard::syncMode()
{
    return _setupPage->syncMode();
    return WizardCommon::BoxMode;
}

void OwncloudWizard::setMultipleFoldersExist(bool exist)
{
    _setupPage->setMultipleFoldersExist(exist);
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
    const int id(currentId());

    switch (id) {
    case WizardCommon::Page_HttpCreds:
        _httpCredsPage->setConnected( enable );
        break;

    case WizardCommon::Page_ShibbolethCreds:
        _shibbolethCredsPage->setConnected( enable );
        break;

    case WizardCommon::Page_oCSetup:
    case WizardCommon::Page_Result:
        qWarning("Should not happen at this stage.");
        break;
    }

    if( enable ) {
        next();
    }
}

void OwncloudWizard::setAuthType(WizardCommon::AuthType type)
{
  _setupPage->setAuthType(type);
  if (type == WizardCommon::Shibboleth) {
    _credentialsPage = _shibbolethCredsPage;
  } else {
    _credentialsPage = _httpCredsPage;
  }
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
    int id(currentId());

    if (id == WizardCommon::Page_oCSetup) {
        _setupPage->setErrorString( msg );
    } else if (id == WizardCommon::Page_HttpCreds) {
        _httpCredsPage->setErrorString(msg);
    } else if (id == WizardCommon::Page_ShibbolethCreds) {
        _shibbolethCredsPage->setErrorString(msg);
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

AbstractCredentials* OwncloudWizard::getCredentials() const
{
  if (_credentialsPage) {
    return _credentialsPage->getCredentials();
  }

  return 0;
}


} // end namespace
