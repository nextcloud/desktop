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

  bool isBusy();

  void writeOwncloudConfig();

  QString mirallConfigFile() const;

  /**
   * returns the configured owncloud url if its already configured, otherwise an empty
   * string.
   */
  QString ownCloudUrl() const ;

signals:

public slots:

protected slots:
  // QProcess related slots:
  void slotReadyReadStandardOutput();
  void slotReadyReadStandardError();
  void slotStateChanged( QProcess::ProcessState );
  void slotError( QProcess::ProcessError );
  void slotStarted();
  void slotFinished( int, QProcess::ExitStatus );

  // wizard dialog signals
  void slotInstallOCServer();
  void slotConnectToOCUrl( const QString& );
  void slotCreateOCLocalhost();

private:
  bool checkOwncloudAdmin( const QString& );
  void runOwncloudAdmin( const QStringList& );

  OwncloudWizard *_ocWizard;
  QProcess       *_process;
};

};

#endif // OWNCLOUDSETUP_H
