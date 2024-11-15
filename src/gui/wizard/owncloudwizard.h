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

#include "libsync/configfile.h"
#include "networkjobs.h"
#include "wizard/owncloudwizardcommon.h"
#include "accountfwd.h"

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcWizard)

class WelcomePage;
class OwncloudSetupPage;
class OwncloudHttpCredsPage;
class OwncloudAdvancedSetupPage;
class OwncloudWizardResultPage;
class AbstractCredentials;
class AbstractCredentialsWizardPage;
class WebViewPage;
class Flow2AuthCredsPage;

/**
 * @brief The OwncloudWizard class
 * @ingroup gui
 */
class OwncloudWizard : public QWizard
{
    Q_OBJECT
public:
    enum LogType {
        LogPlain,
        LogParagraph
    };

    OwncloudWizard(QWidget *parent = nullptr);

    void setAccount(AccountPtr account);
    [[nodiscard]] AccountPtr account() const;
    void setOCUrl(const QString &);
    bool registration();
    void setRegistration(bool registration);

    void setupCustomMedia(QVariant, QLabel *);
    [[nodiscard]] QString ocUrl() const;
    [[nodiscard]] QString localFolder() const;
    [[nodiscard]] QStringList selectiveSyncBlacklist() const;
    [[nodiscard]] bool useFlow2() const;
    [[nodiscard]] bool useVirtualFileSync() const;
    [[nodiscard]] bool isConfirmBigFolderChecked() const;

    void displayError(const QString &, bool retryHTTPonly);
    [[nodiscard]] AbstractCredentials *getCredentials() const;

    void bringToTop();
    void centerWindow();

    /**
     * Shows a dialog explaining the virtual files mode and warning about it
     * being experimental. Calls the callback with true if enabling was
     * chosen.
     */
    static void askExperimentalVirtualFilesFeature(QWidget *receiver, const std::function<void(bool enable)> &callback);

    // FIXME: Can those be local variables?
    // Set from the OwncloudSetupPage, later used from OwncloudHttpCredsPage
    QByteArray _clientCertBundle; // raw, potentially encrypted pkcs12 bundle provided by the user
    QByteArray _clientCertPassword; // password for the pkcs12
    QSslKey _clientSslKey; // key extracted from pkcs12
    QSslCertificate _clientSslCertificate; // cert extracted from pkcs12
    QList<QSslCertificate> _clientSslCaCertificates;

public slots:
    void setAuthType(OCC::DetermineAuthTypeJob::AuthType type);
    void setRemoteFolder(const QString &);
    void appendToConfigurationLog(const QString &msg, OCC::OwncloudWizard::LogType type = LogParagraph);
    void slotCurrentPageChanged(int);
    void successfulStep();
    void slotCustomButtonClicked(const int which);
    void adjustWizardSize();

signals:
    void clearPendingRequests();
    void determineAuthType(const QString &);
    void connectToOCUrl(const QString &);
    void createLocalAndRemoteFolders(const QString &, const QString &);
    // make sure to connect to this, rather than finished(int)!!
    void basicSetupFinished(int);
    void skipFolderConfiguration();
    void needCertificate();
    void styleChanged();
    void onActivate();
    void wizardClosed();

protected:
    void changeEvent(QEvent *) override;
    void hideEvent(QHideEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    void customizeStyle();
    [[nodiscard]] QSize calculateLargestSizeOfWizardPages(const QList<QSize> &pageSizes) const;
    [[nodiscard]] QList<QSize> calculateWizardPageSizes() const;

    AccountPtr _account;
    WelcomePage *_welcomePage;
    OwncloudSetupPage *_setupPage;
    OwncloudHttpCredsPage *_httpCredsPage;
    Flow2AuthCredsPage *_flow2CredsPage;
    OwncloudAdvancedSetupPage *_advancedSetupPage;
    OwncloudWizardResultPage *_resultPage = nullptr;
    AbstractCredentialsWizardPage *_credentialsPage = nullptr;
    WebViewPage *_webViewPage = nullptr;

    QStringList _setupLog;

    bool _registration = false;

    bool _useFlow2 = ConfigFile().forceLoginV2();

    friend class OwncloudSetupWizard;
};

} // namespace OCC

#endif
