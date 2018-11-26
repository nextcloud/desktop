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

#include "account.h"
#include "configfile.h"
#include "theme.h"

#include "wizard/owncloudwizard.h"
#include "wizard/owncloudsetuppage.h"
#include "wizard/owncloudhttpcredspage.h"
#include "wizard/owncloudoauthcredspage.h"
#ifndef NO_SHIBBOLETH
#include "wizard/owncloudshibbolethcredspage.h"
#endif
#include "wizard/owncloudadvancedsetuppage.h"
#include "wizard/owncloudwizardresultpage.h"
#include "wizard/webviewpage.h"

#include "QProgressIndicator.h"

#include <QtCore>
#include <QtGui>

#include <stdlib.h>

namespace OCC {

Q_LOGGING_CATEGORY(lcWizard, "nextcloud.gui.wizard", QtInfoMsg)

OwncloudWizard::OwncloudWizard(QWidget *parent)
    : QWizard(parent)
    , _account(nullptr)
    , _setupPage(new OwncloudSetupPage(this))
    , _httpCredsPage(new OwncloudHttpCredsPage(this))
    , _browserCredsPage(new OwncloudOAuthCredsPage)
#ifndef NO_SHIBBOLETH
    , _shibbolethCredsPage(new OwncloudShibbolethCredsPage)
#endif
    , _advancedSetupPage(new OwncloudAdvancedSetupPage)
    , _resultPage(new OwncloudWizardResultPage)
    , _credentialsPage(nullptr)
    , _webViewPage(new WebViewPage(this))
    , _setupLog()
    , _registration(false)
{
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setPage(WizardCommon::Page_ServerSetup, _setupPage);
    setPage(WizardCommon::Page_HttpCreds, _httpCredsPage);
    setPage(WizardCommon::Page_OAuthCreds, _browserCredsPage);
#ifndef NO_SHIBBOLETH
    setPage(WizardCommon::Page_ShibbolethCreds, _shibbolethCredsPage);
#endif
    setPage(WizardCommon::Page_AdvancedSetup, _advancedSetupPage);
    setPage(WizardCommon::Page_Result, _resultPage);
    setPage(WizardCommon::Page_WebView, _webViewPage);

    connect(this, &QDialog::finished, this, &OwncloudWizard::basicSetupFinished);

    // note: start Id is set by the calling class depending on if the
    // welcome text is to be shown or not.
    setWizardStyle(QWizard::ModernStyle);

    connect(this, &QWizard::currentIdChanged, this, &OwncloudWizard::slotCurrentPageChanged);
    connect(_setupPage, &OwncloudSetupPage::determineAuthType, this, &OwncloudWizard::determineAuthType);
    connect(_httpCredsPage, &OwncloudHttpCredsPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);
    connect(_browserCredsPage, &OwncloudOAuthCredsPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);
#ifndef NO_SHIBBOLETH
    connect(_shibbolethCredsPage, &OwncloudShibbolethCredsPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);
#endif
    connect(_webViewPage, &WebViewPage::connectToOCUrl, this, &OwncloudWizard::connectToOCUrl);
    connect(_advancedSetupPage, &OwncloudAdvancedSetupPage::createLocalAndRemoteFolders,
        this, &OwncloudWizard::createLocalAndRemoteFolders);
    connect(this, &QWizard::customButtonClicked, this, &OwncloudWizard::skipFolderConfiguration);


    Theme *theme = Theme::instance();
    setWindowTitle(tr("%1 Connection Wizard").arg(theme->appNameGUI()));
    setWizardStyle(QWizard::ModernStyle);
    setPixmap(QWizard::BannerPixmap, theme->wizardHeaderBanner());
    setPixmap(QWizard::LogoPixmap, theme->wizardHeaderLogo());
    setOption(QWizard::NoBackButtonOnStartPage);
    setOption(QWizard::NoBackButtonOnLastPage);
    setOption(QWizard::NoCancelButton);
    setTitleFormat(Qt::RichText);
    setSubTitleFormat(Qt::RichText);
    setButtonText(QWizard::CustomButton1, tr("Skip folders configuration"));
}

void OwncloudWizard::setAccount(AccountPtr account)
{
    _account = account;
}

AccountPtr OwncloudWizard::account() const
{
    return _account;
}

QString OwncloudWizard::localFolder() const
{
    return (_advancedSetupPage->localFolder());
}

QStringList OwncloudWizard::selectiveSyncBlacklist() const
{
    return _advancedSetupPage->selectiveSyncBlacklist();
}

bool OwncloudWizard::isConfirmBigFolderChecked() const
{
    return _advancedSetupPage->isConfirmBigFolderChecked();
}

QString OwncloudWizard::ocUrl() const
{
    QString url = field("OCUrl").toString().simplified();
    return url;
}

bool OwncloudWizard::registration() {
    return _registration;
}

void OwncloudWizard::setRegistration(bool registration) {
    _registration = registration;
}


void OwncloudWizard::enableFinishOnResultWidget(bool enable)
{
    _resultPage->setComplete(enable);
}

void OwncloudWizard::setRemoteFolder(const QString &remoteFolder)
{
    _advancedSetupPage->setRemoteFolder(remoteFolder);
    _resultPage->setRemoteFolder(remoteFolder);
}

void OwncloudWizard::successfulStep()
{
    const int id(currentId());

    switch (id) {
    case WizardCommon::Page_HttpCreds:
        _httpCredsPage->setConnected();
        break;

    case WizardCommon::Page_OAuthCreds:
        _browserCredsPage->setConnected();
        break;

#ifndef NO_SHIBBOLETH
    case WizardCommon::Page_ShibbolethCreds:
        _shibbolethCredsPage->setConnected();
        break;
#endif

    case WizardCommon::Page_WebView:
        _webViewPage->setConnected();
        break;

    case WizardCommon::Page_AdvancedSetup:
        _advancedSetupPage->directoriesCreated();
        break;

    case WizardCommon::Page_ServerSetup:
    case WizardCommon::Page_Result:
        qCWarning(lcWizard, "Should not happen at this stage.");
        break;
    }

    next();
}

void OwncloudWizard::setAuthType(DetermineAuthTypeJob::AuthType type)
{
    _setupPage->setAuthType(type);
#ifndef NO_SHIBBOLETH
    if (type == DetermineAuthTypeJob::Shibboleth) {
        _credentialsPage = _shibbolethCredsPage;
    } else
#endif
        if (type == DetermineAuthTypeJob::OAuth) {
        _credentialsPage = _browserCredsPage;
    } else if (type == DetermineAuthTypeJob::WebViewFlow) {
        _credentialsPage = _webViewPage;
    } else { // try Basic auth even for "Unknown"
        _credentialsPage = _httpCredsPage;
    }
    next();
}

// TODO: update this function
void OwncloudWizard::slotCurrentPageChanged(int id)
{
    qCDebug(lcWizard) << "Current Wizard page changed to " << id;

    if (id == WizardCommon::Page_ServerSetup) {
        emit clearPendingRequests();
    }

    if (id == WizardCommon::Page_Result) {
        disconnect(this, &QDialog::finished, this, &OwncloudWizard::basicSetupFinished);
        emit basicSetupFinished(QDialog::Accepted);
        appendToConfigurationLog(QString());
        // Immediately close on show, we currently don't want this page anymore
        done(Accepted);
    }

    setOption(QWizard::HaveCustomButton1, id == WizardCommon::Page_AdvancedSetup);
    if (id == WizardCommon::Page_AdvancedSetup && _credentialsPage == _browserCredsPage) {
        // For OAuth, disable the back button in the Page_AdvancedSetup because we don't want
        // to re-open the browser.
        button(QWizard::BackButton)->setEnabled(false);
    }
}

void OwncloudWizard::displayError(const QString &msg, bool retryHTTPonly)
{
    switch (currentId()) {
    case WizardCommon::Page_ServerSetup:
        _setupPage->setErrorString(msg, retryHTTPonly);
        break;

    case WizardCommon::Page_HttpCreds:
        _httpCredsPage->setErrorString(msg);
        break;

    case WizardCommon::Page_AdvancedSetup:
        _advancedSetupPage->setErrorString(msg);
        break;
    }
}

void OwncloudWizard::appendToConfigurationLog(const QString &msg, LogType /*type*/)
{
    _setupLog << msg;
    qCDebug(lcWizard) << "Setup-Log: " << msg;
}

void OwncloudWizard::setOCUrl(const QString &url)
{
    _setupPage->setServerUrl(url);
}

AbstractCredentials *OwncloudWizard::getCredentials() const
{
    if (_credentialsPage) {
        return _credentialsPage->getCredentials();
    }

    return nullptr;
}

} // end namespace
