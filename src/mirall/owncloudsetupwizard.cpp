/*
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

#include <QtCore>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>

#include "mirall/owncloudsetupwizard.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudinfo.h"
#include "mirall/folderman.h"
#include "mirall/credentialstore.h"
#include "mirall/utility.h"

namespace Mirall {

class Theme;

OwncloudSetupWizard::OwncloudSetupWizard( FolderMan *folderMan, Theme *theme, QObject *parent ) :
    QObject( parent ),
    _mkdirRequestReply(0),
    _checkInstallationRequest(0),
    _folderMan(folderMan)
{
    _ocWizard = new OwncloudWizard;

    connect( _ocWizard, SIGNAL(connectToOCUrl( const QString& ) ),
             this, SLOT(slotConnectToOCUrl( const QString& )));

    connect( _ocWizard, SIGNAL(finished(int)),this,SLOT(slotAssistantFinished(int)));

    connect( _ocWizard, SIGNAL(clearPendingRequests()),
             this, SLOT(slotClearPendingRequests()));

    _ocWizard->setWindowTitle( tr("%1 Connection Wizard").arg( theme ? theme->appNameGUI() : QLatin1String("Mirall") ) );

}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    // delete _ocWizard; FIXME: this crashes!
}

OwncloudWizard *OwncloudSetupWizard::wizard() {
    return _ocWizard;
}

void OwncloudSetupWizard::startWizard()
{
    // Set useful default values.
    MirallConfigFile cfgFile;
    // Fill the entry fields with existing values.
    QString url = cfgFile.ownCloudUrl();
    if( !url.isEmpty() ) {
        _ocWizard->setOCUrl( url );
    }
    QString user = cfgFile.ownCloudUser();
    if( !user.isEmpty() ) {
        _ocWizard->setOCUser( user );
    }

    _remoteFolder = Theme::instance()->defaultServerFolder();
    // remoteFolder may be empty, which means /

    QString localFolder = Theme::instance()->defaultClientFolder();

    // if its a relative path, prepend with users home dir, otherwise use as absolute path
    if( !localFolder.startsWith(QLatin1Char('/')) ) {
        localFolder = QDir::homePath() + QDir::separator() + Theme::instance()->defaultClientFolder();
    }
    _ocWizard->setProperty("localFolder", localFolder);
    _ocWizard->setRemoteFolder(_remoteFolder);

    _ocWizard->setStartId(OwncloudWizard::Page_oCSetup);

    _ocWizard->restart();
    _ocWizard->show();
}


// Method executed when the user ends the wizard, either with 'accept' or 'reject'.
// accept the custom config to be the main one if Accepted.
void OwncloudSetupWizard::slotAssistantFinished( int result )
{
    MirallConfigFile cfg( _configHandle );


    if( result == QDialog::Rejected ) {
        // the old config remains valid. Remove the temporary one.
        cfg.cleanupCustomConfig();
        qDebug() << "Rejected the new config, use the old!";
    } else if( result == QDialog::Accepted ) {
        qDebug() << "Config Changes were accepted!";

        // go through all folders and remove the journals if the server changed.
        MirallConfigFile prevCfg;
        if( prevCfg.ownCloudUrl() != cfg.ownCloudUrl() ) {
            qDebug() << "ownCloud URL has changed, journals needs to be wiped.";
            _folderMan->wipeAllJournals();
        }

        // save the user credentials and afterwards clear the cred store.
        cfg.acceptCustomConfig();

        // Now write the resulting folder definition if folder names are set.
        const QString localFolder = _ocWizard->property("localFolder").toString();
        if( !( localFolder.isEmpty() || _remoteFolder.isEmpty() ) ) { // both variables are set.
            if( _folderMan ) {
                _folderMan->addFolderDefinition( QLatin1String("owncloud"), Theme::instance()->appName(),
                localFolder, _remoteFolder, false );
                _ocWizard->appendToConfigurationLog(tr("<font color=\"green\"><b>Local sync folder %1 successfully created!</b></font>").arg(localFolder));
            } else {
                qDebug() << "WRN: Folderman is zero in Setup Wizzard.";
            }
        }
    } else {
        qDebug() << "WRN: Got unknown dialog result code " << result;
    }

    // clear the custom config handle
    _configHandle.clear();
    ownCloudInfo::instance()->setCustomConfigHandle( QString::null );

    // notify others.
    emit ownCloudWizardDone( result );
}

void OwncloudSetupWizard::slotConnectToOCUrl( const QString& url )
{
  qDebug() << "Connect to url: " << url;
  _ocWizard->setField(QLatin1String("OCUrl"), url );
  _ocWizard->appendToConfigurationLog(tr("Trying to connect to %1 at %2...")
                                  .arg( Theme::instance()->appNameGUI() ).arg(url) );
  testOwnCloudConnect();
}

void OwncloudSetupWizard::slotClearPendingRequests()
{
    qDebug() << "Pending request: " << _mkdirRequestReply;
    if( _mkdirRequestReply && _mkdirRequestReply->isRunning() ) {
        qDebug() << "ABORTing pending mkdir request.";
        _mkdirRequestReply->abort();
    }
    if( _checkInstallationRequest && _checkInstallationRequest->isRunning() ) {
        qDebug() << "ABORTing pending check installation request.";
        _checkInstallationRequest->abort();
    }
    if( _checkRemoteFolderRequest && _checkRemoteFolderRequest->isRunning() ) {
        qDebug() << "ABORTing pending remote folder check request.";
        _checkRemoteFolderRequest->abort();
    }
}

void OwncloudSetupWizard::testOwnCloudConnect()
{
    // write a temporary config.
    QDateTime now = QDateTime::currentDateTime();

    // remove a possibly existing custom config.
    if( ! _configHandle.isEmpty() ) {
        // remove the old config file.
        MirallConfigFile oldConfig( _configHandle );
        oldConfig.cleanupCustomConfig();
    }

    _configHandle = now.toString(QLatin1String("MMddyyhhmmss"));

    MirallConfigFile cfgFile( _configHandle );
    QString url = _ocWizard->field(QLatin1String("OCUrl")).toString();
    if( url.isEmpty() ) return;
    if( !( url.startsWith(QLatin1String("https://")) || url.startsWith(QLatin1String("http://"))) ) {
        qDebug() << "url does not start with a valid protocol, assuming https.";
        url.prepend(QLatin1String("https://"));
        // FIXME: give a hint about the auto completion
        _ocWizard->setOCUrl(url);
    }
    cfgFile.writeOwncloudConfig( Theme::instance()->appName(),
                                 url,
                                 _ocWizard->field(QLatin1String("OCUser")).toString(),
                                 _ocWizard->field(QLatin1String("OCPasswd")).toString() );

    // If there is already a config, take its proxy config.
    if( ownCloudInfo::instance()->isConfigured() ) {
        MirallConfigFile prevCfg;
        if( prevCfg.proxyType() != QNetworkProxy::DefaultProxy ) {
            cfgFile.setProxyType( prevCfg.proxyType(), prevCfg.proxyHostName(), prevCfg.proxyPort(),
                                  prevCfg.proxyUser(), prevCfg.proxyPassword() );
        }
    }

    // now start ownCloudInfo to check the connection.
    ownCloudInfo* info = ownCloudInfo::instance();
    info->setCustomConfigHandle( _configHandle );
    if( info->isConfigured() ) {
        // reset the SSL Untrust flag to let the SSL dialog appear again.
        info->resetSSLUntrust();
        connect(info, SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
                SLOT(slotOwnCloudFound(QString,QString,QString,QString)));
        connect(info, SIGNAL(noOwncloudFound(QNetworkReply*)),
                SLOT(slotNoOwnCloudFound(QNetworkReply*)));
        _checkInstallationRequest = info->checkInstallation();
    } else {
        qDebug() << "   ownCloud seems not to be configured, can not start test connect.";
    }
}

void OwncloudSetupWizard::slotOwnCloudFound( const QString& url, const QString& infoString, const QString& version, const QString& )
{
    disconnect(ownCloudInfo::instance(), SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
               this, SLOT(slotOwnCloudFound(QString,QString,QString,QString)));
    disconnect(ownCloudInfo::instance(), SIGNAL(noOwncloudFound(QNetworkReply*)),
               this, SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    _ocWizard->appendToConfigurationLog(tr("<font color=\"green\">Successfully connected to %1: %2 version %3 (%4)</font><br/><br/>")
                                    .arg( url ).arg(Theme::instance()->appNameGUI()).arg(infoString).arg(version));

    // enable the finish button.
    _ocWizard->button( QWizard::FinishButton )->setEnabled( true );

    // start the local folder creation
    setupLocalSyncFolder();
}

void OwncloudSetupWizard::slotNoOwnCloudFound( QNetworkReply *err )
{
    disconnect(ownCloudInfo::instance(), SIGNAL(ownCloudInfoFound(QString,QString,QString,QString)),
               this, SLOT(slotOwnCloudFound(QString,QString,QString,QString)));
    disconnect(ownCloudInfo::instance(), SIGNAL(noOwncloudFound(QNetworkReply*)),
               this, SLOT(slotNoOwnCloudFound(QNetworkReply*)));

    _ocWizard->displayError(tr("Failed to connect to %1:<br/>%2").
                            arg(Theme::instance()->appNameGUI()).arg(err->errorString()));

    // remove the config file again
    MirallConfigFile cfgFile( _configHandle );
    cfgFile.cleanupCustomConfig();
    finalizeSetup( false );
}

void OwncloudSetupWizard::setupLocalSyncFolder()
{
    if( ! _folderMan ) return;

    const QString localFolder = _ocWizard->property("localFolder").toString();
    qDebug() << "Setup local sync folder for new oC connection " << localFolder;
    QDir fi( localFolder );
    // FIXME: Show problems with local folder properly.

    bool localFolderOk = true;
    if( fi.exists() ) {
        // there is an existing local folder. If its non empty, it can only be synced if the
        // ownCloud is newly created.
        _ocWizard->appendToConfigurationLog( tr("Local sync folder %1 already exists, setting it up for sync.<br/><br/>").arg(localFolder));
    } else {
        QString res = tr("Creating local sync folder %1... ").arg(localFolder);
        if( fi.mkpath( localFolder ) ) {
            Utility::setupFavLink( localFolder );
            // FIXME: Create a local sync folder.
            res += tr("ok");
        } else {
            res += tr("failed.");
            qDebug() << "Failed to create " << fi.path();
            localFolderOk = false;
            _ocWizard->displayError(tr("Could not create local folder %1").arg(localFolder));
        }
        _ocWizard->appendToConfigurationLog( res );
    }

    if( localFolderOk ) {
        checkRemoteFolder();
    }
}

void OwncloudSetupWizard::checkRemoteFolder()
{
    connect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheckReply(QString,QNetworkReply*)));

    qDebug() << "# checking for authentication settings.";
    ownCloudInfo::instance()->setCustomConfigHandle(_configHandle);
    _checkRemoteFolderRequest = ownCloudInfo::instance()->getRequest(_remoteFolder, true ); // this call needs to be authenticated.
    // continue in slotAuthCheckReply
}

void OwncloudSetupWizard::slotAuthCheckReply( const QString&, QNetworkReply *reply )
{
    // disconnect from ownCloud Info signals
    disconnect( ownCloudInfo::instance(),SIGNAL(ownCloudDirExists(QString,QNetworkReply*)),
             this,SLOT(slotAuthCheckReply(QString,QNetworkReply*)));

    bool ok = true;
    QString error;
    QNetworkReply::NetworkError errId = reply->error();

    if( errId == QNetworkReply::NoError ) {
        qDebug() << "******** Remote folder found, all cool!";
    } else if( errId == QNetworkReply::AuthenticationRequiredError ) { // returned if the user is wrong.
        qDebug() << "******** Password is wrong!";
        error = tr("Credentials are wrong!");
        ok = false;
    } else if( errId == QNetworkReply::OperationCanceledError ) {
        // the username was wrong and ownCloudInfo was closing the request after a couple of auth tries.
        qDebug() << "******** Username or password is wrong!";
        error = tr("Username or password is wrong!");
        ok = false;
    } else if( errId == QNetworkReply::ContentNotFoundError ) {
        // FIXME try to create the remote folder!
        if( !createRemoteFolder() ) {
            error = tr("The remote folder could not be accessed!");
            ok = false;
        } else {
            return; // Finish here, the mkdir request will go on.
        }
    } else {
        error = tr("Error: %1").arg(reply->errorString());
        ok = false;
    }

    if( !ok ) {
        _ocWizard->displayError(error);
    } else {
        _ocWizard->setRemoteFolder( _remoteFolder );
    }

    finalizeSetup( ok );
}

bool OwncloudSetupWizard::createRemoteFolder()
{
    if( _remoteFolder.isEmpty() ) return false;

    _ocWizard->appendToConfigurationLog( tr("creating folder on ownCloud: %1" ).arg( _remoteFolder ));
    connect(ownCloudInfo::instance(), SIGNAL(webdavColCreated(QNetworkReply::NetworkError)),
            this, SLOT(slotCreateRemoteFolderFinished(QNetworkReply::NetworkError)));

    _mkdirRequestReply = ownCloudInfo::instance()->mkdirRequest( _remoteFolder );

    return (_mkdirRequestReply != NULL);
}

void OwncloudSetupWizard::slotCreateRemoteFolderFinished( QNetworkReply::NetworkError error )
{
    qDebug() << "** webdav mkdir request finished " << error;
    disconnect(ownCloudInfo::instance(), SIGNAL(webdavColCreated(QNetworkReply::NetworkError)),
               this, SLOT(slotCreateRemoteFolderFinished(QNetworkReply::NetworkError)));

    bool success = true;

    if( error == QNetworkReply::NoError ) {
        _ocWizard->appendToConfigurationLog( tr("Remote folder %1 created successfully.").arg(_remoteFolder));
    } else if( error == 202 ) {
        _ocWizard->appendToConfigurationLog( tr("The remote folder %1 already exists. Connecting it for syncing.").arg(_remoteFolder));
    } else if( error > 202 && error < 300 ) {
        _ocWizard->displayError( tr("The folder creation resulted in HTTP error code %1").arg((int)error ));

        _ocWizard->appendToConfigurationLog( tr("The folder creation resulted in HTTP error code %1").arg((int)error) );
    } else if( error == QNetworkReply::OperationCanceledError ) {
        _ocWizard->displayError( tr("The remote folder creation failed because the provided credentials "
                                    "are wrong!"
                                    "<br/>Please go back and check your credentials.</p>"));
        _ocWizard->appendToConfigurationLog( tr("<p><font color=\"red\">Remote folder creation failed probably because the provided credentials are wrong.</font>"
                                            "<br/>Please go back and check your credentials.</p>"));
        _remoteFolder.clear();
        success = false;
    } else {
        _ocWizard->appendToConfigurationLog( tr("Remote folder %1 creation failed with error <tt>%2</tt>.").arg(_remoteFolder).arg(error));
        _ocWizard->displayError( tr("Remote folder %1 creation failed with error <tt>%2</tt>.").arg(_remoteFolder).arg(error) );
        _remoteFolder.clear();
        success = false;
    }

    finalizeSetup( success );
}

void OwncloudSetupWizard::finalizeSetup( bool success )
{
    // enable/disable the finish button.
    _ocWizard->enableFinishOnResultWidget(success);

    const QString localFolder = _ocWizard->property("localFolder").toString();
    if( success ) {
        if( !(localFolder.isEmpty() || _remoteFolder.isEmpty() )) {
            _ocWizard->appendToConfigurationLog( tr("A sync connection from %1 to remote directory %2 was set up.")
                                             .arg(localFolder).arg(_remoteFolder));
        }
        _ocWizard->appendToConfigurationLog( QLatin1String(" "));
        _ocWizard->appendToConfigurationLog( QLatin1String("<p><font color=\"green\"><b>")
                                         + tr("Successfully connected to %1!")
                                         .arg(Theme::instance()->appNameGUI())
                                         + QLatin1String("</b></font></p>"));
    } else {
        _ocWizard->appendToConfigurationLog(QLatin1String("<p><font color=\"red\">")
                                        + tr("Connection to %1 could not be established. Please check again.")
                                        .arg(Theme::instance()->appNameGUI())
                                        + QLatin1String("</font></p>"));
    }
    _ocWizard->successfullyConnected(success);
}

}
