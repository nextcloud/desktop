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
#include "mirall/sitecopyconfig.h"
#include "mirall/sitecopyfolder.h"
#include "mirall/mirallwebdav.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall {

OwncloudSetupWizard::OwncloudSetupWizard( QObject *parent ) :
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

void OwncloudSetupWizard::slotConnectToOCUrl( const QString& url )
{
  qDebug() << "Connect to url: " << url;
  _ocWizard->setField("OCUrl", url );
  _ocWizard->appendToResultWidget(tr("Connecting to ownCloud at %1").arg(url ));
  slotFinished( 0, QProcess::NormalExit );
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
    slotFinished( 1, QProcess::NormalExit );
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

void OwncloudSetupWizard::slotFinished( int res, QProcess::ExitStatus )
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

    // FIXME: Write the owncloud config via MirallConfigFile
    MirallConfigFile cfgFile;
    cfgFile.writeOwncloudConfig( QString::fromLocal8Bit("ownCloud"),
                                 _ocWizard->field("OCUrl").toString(),
                                 _ocWizard->field( "OCUser").toString(),
                                 _ocWizard->field("OCPasswd").toString() );
    emit ownCloudSetupFinished( true );
    // setupLocalSyncFolder();
  }
}

void OwncloudSetupWizard::startWizard()
{
    MirallConfigFile cfgFile;

    _ocWizard->setOCUrl( cfgFile.ownCloudUrl() );
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
        // FIXME: Create a local sync folder.
    } else {
      qDebug() << "Failed to create " << fi.path();
    }
  }
}

void OwncloudSetupWizard::startFetchFromOC( const QString& syncFolder )
{
    qCritical( "Fetch not longer support, use full sync!" );
}


}
#include "owncloudsetupwizard.moc"
