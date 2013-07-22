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

#include "mirall/wizard/owncloudwizard.h"
#include "mirall/theme.h"

namespace Mirall {

class SiteCopyFolder;
class SyncResult;
class ownCloudInfo;

class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    explicit OwncloudSetupWizard(QObject *parent = 0 );

    ~OwncloudSetupWizard();

    /**
     * @intro wether or not to show the intro wizard page
     */
    void startWizard();

    void installServer();

    bool isBusy();

    void writeOwncloudConfig();

    /**
   * returns the configured owncloud url if its already configured, otherwise an empty
   * string.
   */

    void    setupLocalSyncFolder();

    OwncloudWizard *wizard();

    /** Run the wizard */
    static void runWizard(QObject *obj, const char* amember, QWidget *parent = 0 );

signals:
    // issued if the oC Setup process (owncloud-admin) is finished.
    void    ownCloudSetupFinished( bool );
    // overall dialog close signal.
    void    ownCloudWizardDone( int );

public slots:

protected slots:
    // wizard dialog signals
    void slotConnectToOCUrl( const QString& );
    void slotDetermineAuthType(const QString&);

private slots:
    void slotCreateRemoteFolderFinished( QNetworkReply::NetworkError );
    void slotAssistantFinished( int );
    void slotClearPendingRequests();
    void slotAuthCheckReply( const QString&, QNetworkReply * );

    void slotOwnCloudFoundAuth( const QString&, const QString&, const QString&, const QString& );
    void slotNoOwnCloudFoundAuth( QNetworkReply* );
    void slotAuthCheckReplyFinished();
private:
    bool createRemoteFolder();
    void checkRemoteFolder();

    void finalizeSetup( bool );

    /* Start a request to the newly installed ownCloud to check the connection */
    void testOwnCloudConnect();

    OwncloudWizard *_ocWizard;
    QPointer<QNetworkReply>  _mkdirRequestReply;
    QPointer<QNetworkReply>  _checkInstallationRequest;
    QPointer<QNetworkReply>  _checkRemoteFolderRequest;

    QString         _configHandle;
    QString         _remoteFolder;
};

}

#endif // OWNCLOUDSETUPWIZARD_H
