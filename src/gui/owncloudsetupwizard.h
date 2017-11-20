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

#include "accountfwd.h"
#include "theme.h"
#include "networkjobs.h"

#include "wizard/owncloudwizardcommon.h"

namespace OCC {

class AccountState;

class OwncloudWizard;

/**
 * @brief The OwncloudSetupWizard class
 * @ingroup gui
 */
class OwncloudSetupWizard : public QObject
{
    Q_OBJECT
public:
    /** Run the wizard */
    static void runWizard(QObject *obj, const char *amember, QWidget *parent = 0);
    static bool bringWizardToFrontIfVisible();
signals:
    // overall dialog close signal.
    void ownCloudWizardDone(int);

private slots:
    void slotCheckServer(const QString &);
    void slotSystemProxyLookupDone(const QNetworkProxy &proxy);

    void slotFindServer();
    void slotFindServerBehindRedirect();
    void slotFoundServer(const QUrl &, const QJsonObject &);
    void slotNoServerFound(QNetworkReply *reply);
    void slotNoServerFoundTimeout(const QUrl &url);

    void slotDetermineAuthType();

    void slotConnectToOCUrl(const QString &);
    void slotAuthError();

    void slotCreateLocalAndRemoteFolders(const QString &, const QString &);
    void slotRemoteFolderExists(QNetworkReply *);
    void slotCreateRemoteFolderFinished(QNetworkReply::NetworkError);
    void slotAssistantFinished(int);
    void slotSkipFolderConfiguration();

private:
    explicit OwncloudSetupWizard(QObject *parent = 0);
    ~OwncloudSetupWizard();
    void startWizard();
    void testOwnCloudConnect();
    void createRemoteFolder();
    void finalizeSetup(bool);
    bool ensureStartFromScratch(const QString &localFolder);
    AccountState *applyAccountChanges();
    bool checkDowngradeAdvised(QNetworkReply *reply);

    OwncloudWizard *_ocWizard;
    QString _initLocalFolder;
    QString _remoteFolder;
};
}

#endif // OWNCLOUDSETUPWIZARD_H
