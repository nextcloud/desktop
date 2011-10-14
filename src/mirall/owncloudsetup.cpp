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

#include "owncloudsetup.h"
#include "mirall/sitecopyconfig.h"
#include "mirall/sitecopyfolder.h"

namespace Mirall {

OwncloudSetup::OwncloudSetup( QObject *parent ) :
    QObject( parent )
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
                     SLOT(slotFinished(int, QProcess::ExitStatus)));


    _ocWizard = new OwncloudWizard();

    connect( _ocWizard, SIGNAL(connectToOCUrl( const QString& ) ),
             this, SLOT(slotConnectToOCUrl( const QString& )));

    connect( _ocWizard, SIGNAL(installOCServer()),
             this, SLOT(slotInstallOCServer()));

    connect( _ocWizard, SIGNAL(installOCLocalhost()),
             this, SLOT(slotCreateOCLocalhost()));

    // in case of cancel, terminate the owncloud-admin script.
    connect( _ocWizard, SIGNAL(rejected()), _process, SLOT(terminate()));

}

void OwncloudSetup::slotConnectToOCUrl( const QString& url )
{
  qDebug() << "Connect to url: " << url;
  _ocWizard->setField("OCUrl", url );
  _ocWizard->appendToResultWidget(tr("Connecting to ownCloud at %1").arg(url ));
  slotFinished( 0, QProcess::NormalExit );
}

bool OwncloudSetup::isBusy()
{
  return _process->state() > 0;
}

 OwncloudWizard *OwncloudSetup::wizard()
 {
   return _ocWizard;
 }

void OwncloudSetup::slotCreateOCLocalhost()
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
  runOwncloudAdmin( args );

  // define
  _ocWizard->setField( "OCUrl", QString( "http://localhost/owncloud/") );
}

void OwncloudSetup::slotInstallOCServer()
{
  if( isBusy() ) {
    qDebug() << "Can not install now, busy. Come back later.";
    return;
  }

  const QString server = _ocWizard->field("ftpUrl").toString();
  const QString user   = _ocWizard->field("ftpUser").toString();
  const QString passwd = _ocWizard->field("ftpPasswd").toString();
  const QString dir; // = _ocWizard->field("ftpDir").toString();

  qDebug() << "Install OC on " << server << " as user " << user;

  QStringList args;
  args << "install";
  args << "--server-type" << "ftp";
  args << "--server"   << server;
  args << "--user"     << user;
  if( ! passwd.isEmpty() ) {
    args << "--password" << passwd;
  }
  if( !dir.isEmpty() ) {
    args << "--ftpdir" << dir;
  }
  runOwncloudAdmin( args );
  _ocWizard->setField( "OCUrl", QString( "%1/owncloud/").arg(_ocWizard->field("myOCDomain").toString() ));
}

void OwncloudSetup::runOwncloudAdmin( const QStringList& args )
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
    slotFinished( 1, QProcess::NormalExit );
  }
}


void OwncloudSetup::slotReadyReadStandardOutput()
{
  QByteArray arr = _process->readAllStandardOutput();
  QTextCodec *codec = QTextCodec::codecForName("UTF-8");
  // render the output to status line
  QString string = codec->toUnicode( arr );
  _ocWizard->appendToResultWidget( string );

}

void OwncloudSetup::slotReadyReadStandardError()
{
  qDebug() << _process->readAllStandardError();
}

void OwncloudSetup::slotStateChanged( QProcess::ProcessState )
{

}

void OwncloudSetup::slotError( QProcess::ProcessError )
{

}

void OwncloudSetup::slotStarted()
{
  _ocWizard->button( QWizard::FinishButton )->setEnabled( false );
  _ocWizard->button( QWizard::BackButton )->setEnabled( false );
   QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
}

void OwncloudSetup::slotFinished( int res, QProcess::ExitStatus )
{
  _ocWizard->button( QWizard::FinishButton )->setEnabled( true );
  _ocWizard->button( QWizard::BackButton)->setEnabled( true );
  QApplication::restoreOverrideCursor();

  if( res ) {
    _ocWizard->appendToResultWidget( tr("<font color=\"red\">Installation of ownCloud failed!</font>") );
    _ocWizard->showOCUrlLabel( false );
    emit ownCloudSetupFinished( false );
  } else {
    // Successful installation. Write the config.
    _ocWizard->appendToResultWidget( tr("<font color=\"green\">Installation of ownCloud succeeded!</font>") );
    _ocWizard->showOCUrlLabel( true );
    writeOwncloudConfig();

    emit ownCloudSetupFinished( true );
    setupLocalSyncFolder();
  }
}

void OwncloudSetup::startWizard()
{
  _ocWizard->setOCUrl( ownCloudUrl() );
  _ocWizard->show();
}

QString OwncloudSetup::mirallConfigFile() const
{
  const QString dir = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/mirall.cfg";
  return dir;
}


void OwncloudSetup::writeOwncloudConfig()
{
  qDebug() << "*** writing mirall config to " << mirallConfigFile();
  QSettings settings( mirallConfigFile(), QSettings::IniFormat);
  QString url = _ocWizard->field("OCUrl").toString();
  if( !url.startsWith("http")) {
    url.prepend( "http://" );
  }
  settings.setValue("ownCloud/url", url );
  settings.setValue("ownCloud/user", _ocWizard->field("OCUser").toString() );
  settings.setValue("ownCloud/password", _ocWizard->field("OCPasswd").toString() );

  settings.sync();
}

/*
 * returns the configured owncloud url if its already configured, otherwise an empty
 * string.
 */
QString OwncloudSetup::ownCloudUrl() const
{
  QSettings settings( mirallConfigFile(), QSettings::IniFormat );
  QString url = settings.value( "ownCloud/url" ).toString();
  qDebug() << "Returning configured owncloud url: " << url;

  return url;
}

QString OwncloudSetup::ownCloudUser() const
{
  QSettings settings( mirallConfigFile(), QSettings::IniFormat );
  QString user = settings.value( "ownCloud/user" ).toString();
  qDebug() << "Returning configured owncloud user: " << user;

  return user;
}

QString OwncloudSetup::ownCloudPasswd() const
{
  QSettings settings( mirallConfigFile(), QSettings::IniFormat );
  QString pwd = settings.value( "ownCloud/password" ).toString();

  return pwd;
}

/*
 *  method to check the if the owncloud admin script is existing
 */
bool OwncloudSetup::checkOwncloudAdmin( const QString& bin )
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

void OwncloudSetup::setupLocalSyncFolder()
{
  const QString syncFolder( QDir::homePath() + "/ownCloud" );
  qDebug() << "Setup local sync folder " << syncFolder;
  QDir fi( syncFolder );
  _ocWizard->appendToResultWidget( tr("creating local sync folder %1").arg(syncFolder) );

  if( fi.exists() ) {
    // there is an existing local folder. If its non empty, it can only be synced if the
    // ownCloud is newly created.
    _ocWizard->appendToResultWidget( tr("Local sync folder %1 already exists, can "
                                        "not automatically create.").arg(syncFolder));
  } else {

    if( fi.mkpath( syncFolder ) ) {
      QString targetPath = "mirall";
      qDebug() << "Successfully created " << fi.path();

      // Create a sitecopy config file
      SitecopyConfig scConfig;

      scConfig.writeSiteConfig( syncFolder, /* local path */
                                "ownCloud", /* _folderWizard->field("OCSiteAlias").toString(),  site alias */
                                ownCloudUrl(),
                                ownCloudUser(),
                                ownCloudPasswd(),
                                targetPath );

      // now there is the sitecopy config. A fetch in to the newly created folder mirrors
      // the files from the ownCloud to local
      startFetchFromOC( syncFolder );


    } else {
      qDebug() << "Failed to create " << fi.path();
    }
  }
}

void OwncloudSetup::startFetchFromOC( const QString& syncFolder )
{
  _scf = new SiteCopyFolder( "ownCloud",
                             syncFolder,
                             QString(),
                             this);
  connect( _scf, SIGNAL( syncFinished( const SyncResult& )),
           SLOT( slotFetchFinished( const SyncResult& )));
  _ocWizard->appendToResultWidget( tr("Starting initial fetch of ownCloud data..."));
  _scf->fetchFromOC();
}

void OwncloudSetup::slotFetchFinished( const SyncResult& res )
{
  qDebug() << "Initial fetch finished!";
  if( res.result() == SyncResult::Error ) {
    _ocWizard->appendToResultWidget( tr("Initial fetch of data failed: ") + res.errorString() );
  } else {
    // fetch of data from ownCloud succeeded.
    _ocWizard->appendToResultWidget( tr("Initial fetch of data succeeded.") );
    _ocWizard->appendToResultWidget( tr("Writing mirall folder setting now.") );
          // create a mirall folder entry.
      // FIXME: folderConfigPath is a method of application object, copied to here.
      const QString folderConfigPath = QDesktopServices::storageLocation(QDesktopServices::DataLocation) + "/folders";

      const QString syncFolder( QDir::homePath() + "/ownCloud" );
      const QString targetPath("/");

      QSettings settings(folderConfigPath + "/ownCloud", QSettings::IniFormat);
      settings.setValue("folder/backend", "sitecopy");
      settings.setValue("folder/path", syncFolder );
      settings.setValue("backend:sitecopy/targetPath", targetPath );
      settings.setValue("backend:sitecopy/alias",  "ownCloud" );
      settings.sync();
  }
  _scf->deleteLater();
}

}
#include "owncloudsetup.moc"
