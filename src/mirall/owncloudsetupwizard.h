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

#ifndef OWNCLOUDSETUPWIZARD_H
#define OWNCLOUDSETUPWIZARD_H

#include <QObject>
#include <QWidget>
#include <QProcess>

#include "mirall/owncloudwizard.h"

namespace Mirall {

class SiteCopyFolder;
class SyncResult;

class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    explicit OwncloudSetupWizard( QObject *parent = 0 );

  void startWizard( );

  void installServer();

  bool isBusy();

  void writeOwncloudConfig();

  void startFetchFromOC( const QString& );

  /**
   * returns the configured owncloud url if its already configured, otherwise an empty
   * string.
   */

  void    setupLocalSyncFolder();

  OwncloudWizard *wizard();

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

#endif // OWNCLOUDSETUPWIZARD_H
