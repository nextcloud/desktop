#ifndef OWNCLOUDDIRCHECK_H
#define OWNCLOUDDIRCHECK_H

#include <QObject>
#include <QNetworkReply>

namespace Mirall {

class ownCloudDirCheck : public QObject
{
    Q_OBJECT
public:
    explicit ownCloudDirCheck(QObject *parent = 0);

  bool checkDirectory( const QString& );

signals:
  void directoryExists( const QString&, bool );

protected slots:
  void slotReplyFinished( QNetworkReply* );

private:
  QNetworkAccessManager *_manager;
  QNetworkReply         *_reply;

};

}

#endif // OWNCLOUDDIRCHECK_H
