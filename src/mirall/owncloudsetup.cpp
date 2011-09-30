#include <QtCore>
#include <QProcess>
#include <QMessageBox>
#include <QDesktopServices>

#include "owncloudsetup.h"

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

  if( checkOwncloudAdmin( bin )) {
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
}

void OwncloudSetup::slotFinished( int res, QProcess::ExitStatus )
{
  _ocWizard->button( QWizard::FinishButton )->setEnabled( true );

  if( res ) {
    _ocWizard->appendToResultWidget( tr("<font color=\"red\">Installation of ownCloud failed!</font>") );
  } else {
    // Successful installation. Write the config.
    _ocWizard->appendToResultWidget( tr("<font color=\"green\">Installation of ownCloud succeeded!</font>") );

    writeOwncloudConfig();
  }
}

void OwncloudSetup::startWizard()
{
  _ocWizard->setOCUrl( ownCloudUrl() );
  _ocWizard->exec();
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
  settings.setValue("ownCloud/url", _ocWizard->field("OCUrl").toString() );
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

}
#include "owncloudsetup.moc"
