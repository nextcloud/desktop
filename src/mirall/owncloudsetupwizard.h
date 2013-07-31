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

#include "mirall/theme.h"

namespace Mirall {

class OwncloudWizard;

class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    /** Run the wizard */
    static void runWizard(QObject *obj, const char* amember, QWidget *parent = 0 );

signals:
    // overall dialog close signal.
    void ownCloudWizardDone( int );

private slots:
    void slotDetermineAuthType(const QString&);
    void slotOwnCloudFoundAuth(const QString&, const QString&, const QString&, const QString&);
    void slotAuthCheckReplyFinished();
    void slotNoOwnCloudFoundAuth(QNetworkReply*);

    void slotConnectToOCUrl(const QString&);
    void slotConnectionCheck(const QString&, QNetworkReply*);

    void slotCreateLocalAndRemoteFolders(const QString&, const QString&);
    void slotAuthCheckReply(const QString&, QNetworkReply*);
    void slotCreateRemoteFolderFinished(QNetworkReply::NetworkError);
    void slotAssistantFinished( int );
    void slotClearPendingRequests();

private:
    explicit OwncloudSetupWizard(QObject *parent = 0 );
    ~OwncloudSetupWizard();

    void startWizard();
    void testOwnCloudConnect();
    void checkRemoteFolder();
    bool createRemoteFolder();
    void finalizeSetup( bool );

    OwncloudWizard* _ocWizard;
    QPointer<QNetworkReply>  _mkdirRequestReply;
    QPointer<QNetworkReply>  _checkInstallationRequest;
    QPointer<QNetworkReply>  _checkRemoteFolderRequest;
    QString _configHandle;
    QString _remoteFolder;
};

}

#endif // OWNCLOUDSETUPWIZARD_H
