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
#include <QNetworkReply>
#include <QPointer>

#include "mirall/owncloudwizard.h"
#include "mirall/theme.h"

namespace Mirall {

class SiteCopyFolder;
class SyncResult;
class ownCloudInfo;
class FolderMan;

class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    explicit OwncloudSetupWizard( FolderMan *folderMan = 0, Theme *theme = 0, QObject *parent = 0 );

    ~OwncloudSetupWizard();

    /**
     * @intro wether or not to show the intro wizard page
     */
    void startWizard(bool intro = false);

    void installServer();

    bool isBusy();

    void writeOwncloudConfig();

    /**
   * returns the configured owncloud url if its already configured, otherwise an empty
   * string.
   */

    void    setupLocalSyncFolder();

    OwncloudWizard *wizard();

signals:
    // issued if the oC Setup process (owncloud-admin) is finished.
    void    ownCloudSetupFinished( bool );
    // overall dialog close signal.
    void    ownCloudWizardDone( int );

public slots:

protected slots:
    // QProcess related slots:
    void slotReadyReadStandardOutput();
    void slotReadyReadStandardError();
    void slotStateChanged( QProcess::ProcessState );
    void slotError( QProcess::ProcessError );
    void slotStarted();
    void slotProcessFinished( int, QProcess::ExitStatus );

    // wizard dialog signals
    void slotInstallOCServer();
    void slotConnectToOCUrl( const QString& );
    void slotCreateOCLocalhost();

    void slotCreateRemoteFolder(bool);

private slots:
    void slotOwnCloudFound( const QString&, const QString&, const QString&, const QString& );
    void slotNoOwnCloudFound( QNetworkReply* );
    void slotCreateRemoteFolderFinished( QNetworkReply::NetworkError );
    void slotAssistantFinished( int );
    void slotClearPendingRequests();

private:
    bool checkOwncloudAdmin( const QString& );
    void runOwncloudAdmin( const QStringList& );
    bool createRemoteFolder( const QString& );
    void finalizeSetup( bool );

    /* Start a request to the newly installed ownCloud to check the connection */
    void testOwnCloudConnect();

    OwncloudWizard *_ocWizard;
    QPointer<QNetworkReply>  _mkdirRequestReply;
    QPointer<QNetworkReply>  _checkInstallationRequest;
    FolderMan      *_folderMan;
    QProcess       *_process;

    QString         _configHandle;
    QString         _localFolder;
    QString         _remoteFolder;
};

}

#endif // OWNCLOUDSETUPWIZARD_H
