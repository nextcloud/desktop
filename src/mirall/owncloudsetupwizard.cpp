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
#include "mirall/mirallwebdav.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/owncloudinfo.h"
#include "mirall/folderman.h"

namespace Mirall {

class Theme;

OwncloudSetupWizard::OwncloudSetupWizard( FolderMan *folderMan, Theme *theme, QObject *parent ) :
    QObject( parent ),
    _ocInfo(0),
    _folderMan(folderMan)
{
    _process = new QProcess( this );

    QObject::connect(_process, SIGNAL(readyReadStandardOutput()),
                     SLOT(slotReadyReadStandardOutput()));

    QObject::connect(_process, SIGNAL(readyReadStandardError()),
                     SLOT(slotReadyReadStandardError()));

    QObject::connect(_process, SIGNAL(stateChanged(QProcess::ProcessState)),
                     SLOT(slotStateChanged(QProcess::ProcessState)));

    QObject::connect(_process, SIGNAL(error(QProcess::ProcessError)),
                     SLOT(slotError(QProcess::ProcessError)));

    QObject::connect(_process, SIGNAL(started()),
                     SLOT(slotStarted()));

    QObject::connect(_process, SIGNAL(finished(int, QProcess::ExitStatus)),
                     SLOT(slotProcessFinished(int, QProcess::ExitStatus)));


    _ocWizard = new OwncloudWizard();

    connect( _ocWizard, SIGNAL(connectToOCUrl( const QString& ) ),
             this, SLOT(slotConnectToOCUrl( const QString& )));

    connect( _ocWizard, SIGNAL(installOCServer()),
             this, SLOT(slotInstallOCServer()));

    connect( _ocWizard, SIGNAL(installOCLocalhost()),
             this, SLOT(slotCreateOCLocalhost()));

    connect( _ocWizard, SIGNAL(finished(int)),this,SIGNAL(ownCloudWizardDone(int)));

    // in case of cancel, terminate the owncloud-admin script.
    connect( _ocWizard, SIGNAL(rejected()), _process, SLOT(terminate()));

    _ocWizard->setWindowTitle( tr("%1 Connection Wizard").arg( theme ? theme->appName() : "Mirall" ) );

    // create the ocInfo object
    _ocInfo = new ownCloudInfo;
    connect(_ocInfo,SIGNAL(ownCloudInfoFound(QString,QString)),SLOT(slotOwnCloudFound(QString,QString)));
    connect(_ocInfo,SIGNAL(noOwncloudFound(QNetworkReply*)),SLOT(slotNoOwnCloudFound(QNetworkReply*)));
}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    delete _ocInfo;
}

void OwncloudSetupWizard::slotConnectToOCUrl( const QString& url )
{
  qDebug() << "Connect to url: " << url;
  _ocWizard->setField("OCUrl", url );
  _ocWizard->appendToResultWidget(tr("Trying to connect to ownCloud at %1...").arg(url ));
  testOwnCloudConnect();
}

void OwncloudSetupWizard::testOwnCloudConnect()
{
    // write a config. Note that it has to be removed on fail?!
    MirallConfigFile cfgFile;
    cfgFile.writeOwncloudConfig( QString::fromLocal8Bit("ownCloud"),
                                 _ocWizard->field("OCUrl").toString(),
                                 _ocWizard->field("OCUser").toString(),
                                 _ocWizard->field("OCPasswd").toString(),
                                 _ocWizard->field("PwdNoLocalStore").toBool() );

    // now start ownCloudInfo to check the connection.
    if( _ocInfo->isConfigured() ) {
        // reset the SSL Untrust flag to let the SSL dialog appear again.
        _ocInfo->resetSSLUntrust();
        _ocInfo->checkInstallation();
    } else {
        qDebug() << "   ownCloud seems not to be configured, can not start test connect.";
    }
}

void OwncloudSetupWizard::slotOwnCloudFound( const QString& url, const QString& infoString )
{
    _ocWizard->appendToResultWidget(tr("<font color=\"green\">Successfully connected to %1: ownCloud version %2</font><br/><br/>").arg( url ).arg(infoString));

    // enable the finish button.
    _ocWizard->button( QWizard::FinishButton )->setEnabled( true );

    // start the local folder creation
    setupLocalSyncFolder();
}

void OwncloudSetupWizard::slotNoOwnCloudFound( QNetworkReply *err )
{
    _ocWizard->appendToResultWidget(tr("<font color=\"red\">Failed to connect to ownCloud!</font>") );
    _ocWizard->appendToResultWidget(tr("Error: <tt>%1</tt>").arg(err->errorString()) );

    // remove the config file again
    MirallConfigFile cfgFile;
    cfgFile.removeConnection();

    // Error detection!

}

bool OwncloudSetupWizard::isBusy()
{
  return _process->state() > 0;
}

 OwncloudWizard *OwncloudSetupWizard::wizard()
 {
   return _ocWizard;
 }

void OwncloudSetupWizard::slotCreateOCLocalhost()
{
  if( isBusy() ) {
    qDebug() << "Can not install now, busy. Come back later.";
    return;
  }

  qDebug() << "Install OC on localhost";

  QStringList args;

  args << "install";
  args << "--server-type" << "local";
  args << "--root_helper" << "kdesu -c";

  const QString adminUser = _ocWizard->field("OCUser").toString();
  const QString adminPwd  = _ocWizard->field("OCPasswd").toString();

  args << "--admin-user" << adminUser;
  args << "--admin-password" << adminPwd;

  runOwncloudAdmin( args );

  // define
  _ocWizard->setField( "OCUrl", QString( "http://localhost/owncloud/") );
}

void OwncloudSetupWizard::slotInstallOCServer()
{
  if( isBusy() ) {
    qDebug() << "Can not install now, busy. Come back later.";
    return;
  }

  const QString server = _ocWizard->field("ftpUrl").toString();
  const QString user   = _ocWizard->field("ftpUser").toString();
  const QString passwd = _ocWizard->field("ftpPasswd").toString();
  const QString adminUser = _ocWizard->field("OCUser").toString();
  const QString adminPwd  = _ocWizard->field("OCPasswd").toString();

  qDebug() << "Install OC on " << server << " as user " << user;

  QStringList args;
  args << "install";
  args << "--server-type" << "ftp";
  args << "--server"   << server;
  args << "--ftp-user"     << user;
  if( ! passwd.isEmpty() ) {
    args << "--ftp-password" << passwd;
  }
  args << "--admin-user" << adminUser;
  args << "--admin-password" << adminPwd;

  runOwncloudAdmin( args );
  _ocWizard->setField( "OCUrl", QString( "%1/owncloud/").arg(_ocWizard->field("myOCDomain").toString() ));
}

void OwncloudSetupWizard::runOwncloudAdmin( const QStringList& args )
{
  const QString bin("/usr/bin/owncloud-admin");
  qDebug() << "starting " << bin << " with args. " << args;
  if( _process->state() != QProcess::NotRunning	) {
    qDebug() << "Owncloud admin is still running, skip!";
    return;
  }
  if( checkOwncloudAdmin( bin )) {
    _ocWizard->appendToResultWidget( tr("Starting script owncloud-admin...") );
    _process->start( bin, args );
  } else {
    slotProcessFinished( 1, QProcess::NormalExit );
  }
}


void OwncloudSetupWizard::slotReadyReadStandardOutput()
{
  QByteArray arr = _process->readAllStandardOutput();
  QTextCodec *codec = QTextCodec::codecForName("UTF-8");
  // render the output to status line
  QString string = codec->toUnicode( arr );
  _ocWizard->appendToResultWidget( string, OwncloudWizard::LogPlain );

}

void OwncloudSetupWizard::slotReadyReadStandardError()
{
  qDebug() << "!! " <<_process->readAllStandardError();
}

void OwncloudSetupWizard::slotStateChanged( QProcess::ProcessState )
{

}

void OwncloudSetupWizard::slotError( QProcess::ProcessError err )
{
  qDebug() << "An Error happend with owncloud-admin: " << err << ", exit-Code: " << _process->exitCode();
}

void OwncloudSetupWizard::slotStarted()
{
  _ocWizard->button( QWizard::FinishButton )->setEnabled( false );
  _ocWizard->button( QWizard::BackButton )->setEnabled( false );
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

/*
 *
 */
void OwncloudSetupWizard::slotProcessFinished( int res, QProcess::ExitStatus )
{
  _ocWizard->button( QWizard::FinishButton )->setEnabled( true );
  _ocWizard->button( QWizard::BackButton)->setEnabled( true );
  QApplication::restoreOverrideCursor();

  qDebug() << "exit code: " << res;
  if( res ) {
    _ocWizard->appendToResultWidget( tr("<font color=\"red\">Installation of ownCloud failed!</font>") );
    _ocWizard->showOCUrlLabel( false );
    emit ownCloudSetupFinished( false );
  } else {
    // Successful installation. Write the config.
    _ocWizard->appendToResultWidget( tr("<font color=\"green\">Installation of ownCloud succeeded!</font>") );
    _ocWizard->showOCUrlLabel( true );

    testOwnCloudConnect();
  }
}

void OwncloudSetupWizard::startWizard()
{
    MirallConfigFile cfgFile;

    QString url = cfgFile.ownCloudUrl();
    if( !url.isEmpty() ) {
        _ocWizard->setOCUrl( url );
    }
    _ocWizard->restart();
    _ocWizard->show();
}


/*
 *  method to check the if the owncloud admin script is existing
 */
bool OwncloudSetupWizard::checkOwncloudAdmin( const QString& bin )
{
  QFileInfo fi( bin );
  qDebug() << "checking owncloud-admin " << bin;
  if( ! (fi.exists() && fi.isExecutable() ) ) {
    _ocWizard->appendToResultWidget( tr("The owncloud admin script can not be found.\n"
      "Setup can not be done.") );
      return false;
  }
  return true;
}

void OwncloudSetupWizard::setupLocalSyncFolder()
{
    _localFolder = QDir::homePath() + QString::fromLocal8Bit("/ownCloud");

    if( ! _folderMan ) return;
    if( _folderMan->map().count() > 0 ) {
        _ocWizard->appendToResultWidget( tr("Skipping automatic setup of sync folders as there are already sync folders.") );
        return;
    }

    qDebug() << "Setup local sync folder " << _localFolder;
    QDir fi( _localFolder );
    _ocWizard->appendToResultWidget( tr("Checking local sync folder %1").arg(_localFolder) );

    bool localFolderOk = true;
    if( fi.exists() ) {
        // there is an existing local folder. If its non empty, it can only be synced if the
        // ownCloud is newly created.
        _ocWizard->appendToResultWidget( tr("Local sync folder %1 already exists, setting it up for sync.<br/><br/>").arg(_localFolder));
    } else {
        QString res = tr("Creating local sync folder %1... ").arg(_localFolder);

        _ocWizard->appendToResultWidget( tr("Creating local sync folder %1").arg(_localFolder));
        if( fi.mkpath( _localFolder ) ) {
            // FIXME: Create a local sync folder.
            res += tr("ok");
        } else {
            res += tr("failed.");
            qDebug() << "Failed to create " << fi.path();
            localFolderOk = false;
        }
    }

    if( localFolderOk ) {
        _remoteFolder = QString::fromLocal8Bit("clientsync"); // FIXME: Themeable
        if( createRemoteFolder( _remoteFolder ) ) {
            // the creation started successfully, does not mean it will work
            qDebug() << "Creation of remote folder started successfully.";
        } else {
            // the start of the http request failed.
            _ocWizard->appendToResultWidget(tr("Start Creation of remote folder %1 failed.").arg(_remoteFolder));
            qDebug() << "Creation of remote folder failed.";
        }

    }
}

bool OwncloudSetupWizard::createRemoteFolder( const QString& folder )
{
    if( folder.isEmpty() ) return false;

    MirallConfigFile cfgFile;

    QString url = cfgFile.ownCloudUrl( cfgFile.defaultConnection(), true );
    url.append( folder );
    qDebug() << "creating folder on ownCloud: " << url;

    MirallWebDAV *webdav = new MirallWebDAV(this);
    connect( webdav, SIGNAL(webdavFinished(QNetworkReply*)),
             SLOT(slotCreateRemoteFolderFinished(QNetworkReply*)));

    webdav->httpConnect( url, cfgFile.ownCloudUser(), cfgFile.ownCloudPasswd() );
    if( webdav->mkdir(  url  ) ) {
        qDebug() << "WebDAV mkdir request successfully started";
        return true;
    } else {
        qDebug() << "WebDAV mkdir request failed";
        return false;
    }
}

void OwncloudSetupWizard::slotCreateRemoteFolderFinished( QNetworkReply *reply )
{
    qDebug() << "** webdav mkdir request finished " << reply->error();

    if( reply->error() == QNetworkReply::NoError ) {
        _ocWizard->appendToResultWidget( tr("Remote folder %1 created sucessfully.").arg(_remoteFolder));

        // Now write the resulting folder definition
        if( _folderMan ) {
            _folderMan->addFolderDefinition("owncloud", "ownCloud", _localFolder, _remoteFolder, false );
            _ocWizard->appendToResultWidget(tr("<font color=\"green\"><b>Local sync folder %1 successfully created!</b></font>").arg(_localFolder));
        }
    } else if( reply->error() == 202 ) {
        _ocWizard->appendToResultWidget(tr("The remote folder %1 already exists. Automatic sync setup is skipped for security reasons. Please configure your sync folder manually.").arg(_remoteFolder));

    } else {
        _ocWizard->appendToResultWidget( tr("Remote folder %1 creation failed with error %2.").arg(_remoteFolder).arg(reply->error()));
    }
}

}
