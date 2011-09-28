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
    connect( _ocWizard, SIGNAL( connectToOCUrl( const QString& ) ),
             this, SLOT( slotConnectToOCUrl( const QString& )));

    connect( _ocWizard, SIGNAL( installOCServer()),
             this, SLOT( slotInstallOCServer()));

}

void OwncloudSetup::slotConnectToOCUrl( const QString& url )
{
  qDebug() << "Connect to url: " << url;
}

void OwncloudSetup::slotInstallServer()
{
  const QString server = _ocWizard->field("ftpUrl").toString();
  const QString user   = _ocWizard->field("ftpUser").toString();
  const QString passwd = _ocWizard->field("ftpPasswd").toString();

  qDebug() << "Connect to " << server << " as user " << user;

  const QString bin( "/home/kf/github/owncloud-admin/bin/owncloud-admin" );
  QStringList args;
  args << "install";
  args << "--server-type" << "ftp";
  args << "--server" << server;
  args << "--user" << user;
  args << "--password" << passwd;
  _process->start( bin, args );
}

void OwncloudSetup::readyReadStandardOutput()
{

}

void OwncloudSetup::readReadStandardError()
{

}

void OwncloudSetup::slotStateChanged( QProcess::ProcessState )
{

}

void OwncloudSetup::slotError( QProcess::ProcessError )
{

}

void OwncloudSetup::slotStarted()
{

}

void OwncloudSetup::slotFinished( int, QProcess::ExitStatus )
{

}

void OwncloudSetup::startWizard( )
{
  _ocWizard->exec();
}

}
