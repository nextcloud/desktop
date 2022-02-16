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
signals:
    // overall dialog close signal.
    void ownCloudWizardDone(int);

private slots:
    void slotCheckServer(const QString &);
    void slotSystemProxyLookupDone(const QNetworkProxy &proxy);

    void slotFindServer();
    void slotFoundServer(const QUrl &, const QJsonObject &);
    void slotNoServerFound(QNetworkReply *reply);
    void slotNoServerFoundTimeout(const QUrl &url);

    void slotDetermineAuthType();

    void slotConnectToOCUrl(const QString &);

    void slotCreateLocalAndRemoteFolders();
    void slotRemoteFolderExists(QNetworkReply *);
    void slotCreateRemoteFolderFinished(QNetworkReply *reply);
    void slotAssistantFinished(int);

private:
    explicit OwncloudSetupWizard(QWidget *parent = nullptr);
    ~OwncloudSetupWizard() override;

    void addFolder(AccountStatePtr account, const QString &localFolder, const QString &remotePath, const QUrl &webDavUrl);
    void startWizard();
    void testOwnCloudConnect();
    void createRemoteFolder();
    void finalizeSetup(bool);
    bool ensureStartFromScratch(const QString &localFolder);
    AccountStatePtr applyAccountChanges();

    OwncloudWizard *_ocWizard;

    friend class ownCloudGui;
};
}

#endif // OWNCLOUDSETUPWIZARD_H
