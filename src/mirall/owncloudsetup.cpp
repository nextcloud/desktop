#include <QtCore>
#include <QProcess>
#include <QMessageBox>

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
    _ocWizard->appendToResultWidget( tr("<font color=\"green\">Installation of ownCloud succeeded!</font>") );
  }
}

void OwncloudSetup::startWizard( )
{
  _ocWizard->exec();
}

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
