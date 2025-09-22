/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#include "wizard/wizardproxysettingsdialog.h"

namespace OCC {

class AccountState;
class TermsOfServiceChecker;

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
    static void runWizard(QObject *obj, const char *amember, QWidget *parent = nullptr);
    static bool bringWizardToFrontIfVisible();

signals:
    // overall dialog close signal.
    void ownCloudWizardDone(int);

private slots:
    void slotCheckServer(const QUrl &serverURL, const OCC::WizardProxySettingsDialog::WizardProxySettings &proxySettings);
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
    void slotCreateRemoteFolderFinished(QNetworkReply *reply);
    void slotAssistantFinished(int);
    void slotSkipFolderConfiguration();

private:
    explicit OwncloudSetupWizard(QObject *parent = nullptr);
    ~OwncloudSetupWizard() override;
    void startWizard();
    void testOwnCloudConnect();
    void createRemoteFolder();
    void finalizeSetup(bool);
    bool ensureStartFromScratch(const QString &localFolder);
    AccountState *applyAccountChanges();
    bool checkDowngradeAdvised(QNetworkReply *reply);

    OwncloudWizard *_ocWizard = nullptr;
    QString _initLocalFolder;
    QString _remoteFolder;
};
}

#endif // OWNCLOUDSETUPWIZARD_H
