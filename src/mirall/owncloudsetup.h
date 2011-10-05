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

#ifndef OWNCLOUDSETUP_H
#define OWNCLOUDSETUP_H

#include <QObject>
#include <QWidget>
#include <QProcess>

#include "mirall/owncloudwizard.h"

namespace Mirall {

class SiteCopyFolder;
class SyncResult;

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

  void startFetchFromOC( const QString& );

  /**
   * returns the configured owncloud url if its already configured, otherwise an empty
   * string.
   */
  QString ownCloudUrl() const ;
  QString ownCloudUser() const ;
  QString ownCloudPasswd() const ;

  void    setupLocalSyncFolder();
signals:
  void    ownCloudSetupFinished( bool );

public slots:

protected slots:
  // QProcess related slots:
  void slotReadyReadStandardOutput();
  void slotReadyReadStandardError();
  void slotStateChanged( QProcess::ProcessState );
  void slotError( QProcess::ProcessError );
  void slotStarted();
  void slotFinished( int, QProcess::ExitStatus );
  void slotFetchFinished( const SyncResult& );

  // wizard dialog signals
  void slotInstallOCServer();
  void slotConnectToOCUrl( const QString& );
  void slotCreateOCLocalhost();

private:
  bool checkOwncloudAdmin( const QString& );
  void runOwncloudAdmin( const QStringList& );

  OwncloudWizard *_ocWizard;
  QProcess       *_process;
  SiteCopyFolder *_scf;
};

};

#endif // OWNCLOUDSETUP_H
