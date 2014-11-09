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

#include "wizard/owncloudwizardcommon.h"

namespace OCC {

class Account;
class OwncloudSetupPage;
class OwncloudHttpCredsPage;
class OwncloudShibbolethCredsPage;
class OwncloudAdvancedSetupPage;
class OwncloudWizardResultPage;
class AbstractCredentials;
class AbstractCredentialsWizardPage;

class OwncloudWizard: public QWizard
{
    Q_OBJECT
public:

    enum LogType {
      LogPlain,
      LogParagraph
    };

    OwncloudWizard(QWidget *parent = 0);

    void setAccount(Account *account);
    Account* account() const;
    void setOCUrl( const QString& );

    void setupCustomMedia( QVariant, QLabel* );
    QString ocUrl() const;
    QString localFolder() const;
    QStringList blacklist() const;

    void enableFinishOnResultWidget(bool enable);

    void displayError( const QString& );
    void setMultipleFoldersExist( bool );
    void setConfigExists( bool );
    bool configExists();
    void successfulStep();
    AbstractCredentials* getCredentials() const;

public slots:
    void setAuthType(WizardCommon::AuthType type);
    void setRemoteFolder( const QString& );
    void appendToConfigurationLog( const QString& msg, LogType type = LogParagraph );
    void slotCurrentPageChanged( int );

signals:
    void clearPendingRequests();
    void determineAuthType(const QString&);
    void connectToOCUrl( const QString& );
    void createLocalAndRemoteFolders(const QString&, const QString&);
    // make sure to connect to this, rather than finished(int)!!
    void basicSetupFinished( int );
    void skipFolderConfiguration();

private:
    Account* _account;
    OwncloudSetupPage* _setupPage;
    OwncloudHttpCredsPage* _httpCredsPage;
    OwncloudShibbolethCredsPage* _shibbolethCredsPage;
    OwncloudAdvancedSetupPage* _advancedSetupPage;
    OwncloudWizardResultPage* _resultPage;
    AbstractCredentialsWizardPage* _credentialsPage;

    QString _configFile;
    QString _oCUser;
    QStringList _setupLog;
    bool _configExists;
};

} // namespace OCC

#endif
