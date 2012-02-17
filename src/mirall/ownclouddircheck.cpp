#include <QtCore>

#include "mirall/ownclouddircheck.h"
#include "mirall/owncloudinfo.h"
#include "mirall/mirallconfigfile.h"

namespace Mirall {

ownCloudDirCheck::ownCloudDirCheck(QObject *parent) :
    QObject(parent),
    _manager( new QNetworkAccessManager ),
    _reply(0)
{
  connect( _manager, SIGNAL(finished(QNetworkReply*)),
           this, SLOT(slotReplyFinished(QNetworkReply*)));
}

bool ownCloudDirCheck::checkDirectory( const QString& dir )
{
  if( dir.isEmpty() ) {
    // assume the root exists on the ownCloud
    emit directoryExists( dir, true );
    return true;
  }

  MirallConfigFile cfgFile;

  if( _reply && _reply->isRunning() ) _reply->abort();

  QNetworkRequest request;
  request.setUrl( QUrl( cfgFile.ownCloudUrl() + "/files/webdav.php/"+dir ) );
  request.setRawHeader( "User-Agent", "mirall" );

  QString concatenated = cfgFile.ownCloudUser() + ":" + cfgFile.ownCloudPasswd();
  QByteArray data = concatenated.toLocal8Bit().toBase64();
  QString headerData = "Basic " + data;
  request.setRawHeader("Authorization", headerData.toLocal8Bit());

  _reply = _manager->get( request );

}

void ownCloudDirCheck::slotReplyFinished( QNetworkReply *reply )
{
  bool re = true;
  if( reply->error() != QNetworkReply::NoError ) {
    qDebug() << "Error in ownCloudDirCheck: " << reply->error();
    re = false;
  }
  qDebug() << "ownCloudDirCheck ret code: " << reply->error();
  emit directoryExists( reply->url().toString(),  re );
}

}
#include "ownclouddircheck.moc"
