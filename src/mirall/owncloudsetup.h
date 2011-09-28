#ifndef OWNCLOUDSETUP_H
#define OWNCLOUDSETUP_H

#include <QObject>
#include <QWidget>
#include <QProcess>

#include "mirall/owncloudwizard.h"

namespace Mirall {


class OwncloudSetup : public QObject
{
    Q_OBJECT
public:
    explicit OwncloudSetup( QObject *parent = 0 );

  void startWizard( );

  void installServer();
signals:

public slots:

protected slots:
  // QProcess related slots:
  void readyReadStandardOutput();
  void readReadStandardError();
  void slotStateChanged( QProcess::ProcessState );
  void slotError( QProcess::ProcessError );
  void slotStarted();
  void slotFinished( int, QProcess::ExitStatus );
  void connectToOCUrl( const QString& );

  // wizard dialog signals
  void slotInstallServer();
  void slotConnectToOCUrl( const QString& );

private:
  OwncloudWizard *_ocWizard;
  QProcess       *_process;
};

};

#endif // OWNCLOUDSETUP_H
