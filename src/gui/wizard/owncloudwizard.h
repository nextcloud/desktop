/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#ifndef MIRALL_OWNCLOUD_WIZARD_H
#define MIRALL_OWNCLOUD_WIZARD_H

#include <QWizard>
#include <QLoggingCategory>
#include <QSslKey>
#include <QSslCertificate>

#include "networkjobs.h"
#include "wizard/owncloudwizardcommon.h"
#include "accountfwd.h"

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcWizard)

class OwncloudSetupPage;
class OwncloudHttpCredsPage;
class OwncloudOAuthCredsPage;
class OwncloudAdvancedSetupPage;
class AbstractCredentials;
class AbstractCredentialsWizardPage;

/**
 * @brief The OwncloudWizard class
 * @ingroup gui
 */
class OwncloudWizard : public QWizard
{
    Q_OBJECT
public:
    OwncloudWizard(QWidget *parent = nullptr);

    void setAccount(AccountPtr account);
    AccountPtr account() const;
    void setOCUrl(const QString &);

    QString ocUrl() const;
    QStringList selectiveSyncBlacklist() const;
    bool useVirtualFileSync() const;
    bool manualFolderConfig() const;
    bool isConfirmBigFolderChecked() const;

    void displayError(const QString &);
    AbstractCredentials *getCredentials() const;

    /**
     * Shows a dialog explaining the virtual files mode and warning about it
     * being experimental. Calles the callback with true if enabling was
     * chosen.
     */
    static void askExperimentalVirtualFilesFeature(QWidget *receiver, const std::function<void(bool enable)> &callback);

    DetermineAuthTypeJob::AuthType authType() const;

    QSize minimumSizeHint() const override;

    void setLocalFolder(const QString &newLocalFolder);
    const QString &localFolder() const;

    void setRemoteFolder(const QString &);
    const QString &remoteFolder() const;
    void resetRemoteFolder();

public slots:
    void setAuthType(DetermineAuthTypeJob::AuthType type);
    void slotCurrentPageChanged(int);
    void successfulStep();

signals:
    void createLocalAndRemoteFolders();
    void clearPendingRequests();
    void determineAuthType(const QString &);
    void connectToOCUrl(const QString &);
    // make sure to connect to this, rather than finished(int)!!
    void basicSetupFinished(int);
    void needCertificate();

private:
    QString _remoteFolder;
    QString _localFolder;
    AccountPtr _account;
    OwncloudSetupPage *_setupPage;
    OwncloudHttpCredsPage *_httpCredsPage;
    OwncloudOAuthCredsPage *_oauthCredsPage;
    OwncloudAdvancedSetupPage *_advancedSetupPage;
    AbstractCredentialsWizardPage *_credentialsPage;
    DetermineAuthTypeJob::AuthType _authType;

    friend class OwncloudSetupWizard;
};

} // namespace OCC

#endif
