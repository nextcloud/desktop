#include <QtCore>
#include <QProcess>

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

  const QString bin( "/usr/bin/owncloud-admin" );
  QStringList args;

  args << "install";
  args << "--server-type" << "local";
  args << "--root_helper" << "kdesu -c";

  _process->start( bin, args );

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

  const QString bin( "/usr/bin/owncloud-admin" );
  QStringList args;
  args << "install";
  args << "--server-type" << "ftp";
  args << "--server"   << server;
  args << "--user"     << user;
  args << "--password" << passwd;
  if( !dir.isEmpty() ) {
    args << "--ftpdir" << dir;
  }
  _process->start( bin, args );
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
    _ocWizard->appendToResultWidget( tr("Installation of ownCloud failed!") );
  } else {
    _ocWizard->appendToResultWidget( tr("Installation of ownCloud succeeded!") );
  }
}

void OwncloudSetup::startWizard( )
{
  _ocWizard->exec();
}

}

#include "owncloudsetup.moc"
