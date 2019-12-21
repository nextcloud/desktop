/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 * Copyright (C) by Michael Schuster <michael@nextcloud.com>
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

#include <QVariant>
#include <QMenu>
#include <QClipboard>

#include "wizard/flow2authcredspage.h"
#include "theme.h"
#include "account.h"
#include "cookiejar.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "creds/credentialsfactory.h"
#include "creds/webflowcredentials.h"

#include "QProgressIndicator.h"

namespace OCC {

Flow2AuthCredsPage::Flow2AuthCredsPage()
    : AbstractCredentialsWizardPage()
    , _ui()
    , _progressIndi(new QProgressIndicator(this))
{
    _ui.setupUi(this);

    Theme *theme = Theme::instance();
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();
    QVariant variant = theme->customMedia(Theme::oCSetupTop);
    WizardCommon::setupCustomMedia(variant, _ui.topLabel);
    variant = theme->customMedia(Theme::oCSetupBottom);
    WizardCommon::setupCustomMedia(variant, _ui.bottomLabel);

    WizardCommon::initErrorLabel(_ui.errorLabel);

    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(Theme::instance()->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Login in your browser (Login Flow v2)")));

    connect(_ui.openLinkButton, &QCommandLinkButton::clicked, this, &Flow2AuthCredsPage::slotOpenBrowser);
    connect(_ui.copyLinkButton, &QCommandLinkButton::clicked, this, &Flow2AuthCredsPage::slotCopyLinkToClipboard);

    _ui.horizontalLayout->addWidget(_progressIndi);
    stopSpinner(false);

    customizeStyle();
}

void Flow2AuthCredsPage::initializePage()
{
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);
    ocWizard->account()->setCredentials(CredentialsFactory::create("http"));
    _asyncAuth.reset(new Flow2Auth(ocWizard->account().data(), this));
    connect(_asyncAuth.data(), &Flow2Auth::result, this, &Flow2AuthCredsPage::asyncAuthResult, Qt::QueuedConnection);
    connect(_asyncAuth.data(), &Flow2Auth::statusChanged, this, &Flow2AuthCredsPage::slotStatusChanged);
    connect(this, &Flow2AuthCredsPage::pollNow, _asyncAuth.data(), &Flow2Auth::slotPollNow);
    _asyncAuth->start();

    // Don't hide the wizard (avoid user confusion)!
    //wizard()->hide();
}

void OCC::Flow2AuthCredsPage::cleanupPage()
{
    // The next or back button was activated, show the wizard again
    wizard()->show();
    _asyncAuth.reset();

    // Forget sensitive data
    _appPassword.clear();
    _user.clear();
}

void Flow2AuthCredsPage::asyncAuthResult(Flow2Auth::Result r, const QString &user,
    const QString &appPassword)
{
    stopSpinner(false);

    switch (r) {
    case Flow2Auth::NotSupported: {
        /* Flow2Auth not supported (can't open browser) */
        _ui.errorLabel->setText(tr("Unable to open the Browser, please copy the link to your Browser."));
        _ui.errorLabel->show();

        /* Don't fallback to HTTP credentials */
        /*OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        ocWizard->back();
        ocWizard->setAuthType(DetermineAuthTypeJob::Basic);*/
        break;
    }
    case Flow2Auth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        _ui.errorLabel->show();
        wizard()->show();
        break;
    case Flow2Auth::LoggedIn: {
        _user = user;
        _appPassword = appPassword;
        OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        Q_ASSERT(ocWizard);

        emit connectToOCUrl(ocWizard->account()->url().toString());
        break;
    }
    }
}

int Flow2AuthCredsPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}

void Flow2AuthCredsPage::setConnected()
{
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);

    // bring wizard to top
    ocWizard->bringToTop();
}

AbstractCredentials *Flow2AuthCredsPage::getCredentials() const
{
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);
    return new WebFlowCredentials(
                _user,
                _appPassword,
                ocWizard->_clientSslCertificate,
                ocWizard->_clientSslKey,
                ocWizard->_clientSslCaCertificates
    );
}

bool Flow2AuthCredsPage::isComplete() const
{
    return false; /* We can never go forward manually */
}

void Flow2AuthCredsPage::slotOpenBrowser()
{
    if (_ui.errorLabel)
        _ui.errorLabel->hide();

    if (_asyncAuth)
        _asyncAuth->openBrowser();
}

void Flow2AuthCredsPage::slotCopyLinkToClipboard()
{
    if (_asyncAuth)
        QApplication::clipboard()->setText(_asyncAuth->authorisationLink().toString(QUrl::FullyEncoded));
}

void Flow2AuthCredsPage::slotPollNow()
{
    emit pollNow();
}

void Flow2AuthCredsPage::slotStatusChanged(int secondsLeft)
{
    const bool pollingNow = (secondsLeft == 0);

    _ui.statusLabel->setText(tr("Polling for authorization") + (pollingNow ? "" : QString(" " + tr("in %1 seconds").arg(secondsLeft))) + "...");

    if(pollingNow)
        startSpinner();
    else
        stopSpinner(true);
}

void Flow2AuthCredsPage::startSpinner()
{
    _ui.horizontalLayout->setEnabled(true);
    _ui.statusLabel->setVisible(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();

    _ui.openLinkButton->setEnabled(false);
    _ui.copyLinkButton->setEnabled(false);
}

void Flow2AuthCredsPage::stopSpinner(bool showStatusLabel)
{
    _ui.horizontalLayout->setEnabled(false);
    _ui.statusLabel->setVisible(showStatusLabel);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();

    _ui.openLinkButton->setEnabled(true);
    _ui.copyLinkButton->setEnabled(true);
}

void Flow2AuthCredsPage::slotStyleChanged()
{
    customizeStyle();
}

void Flow2AuthCredsPage::customizeStyle()
{
    if(_progressIndi)
        _progressIndi->setColor(QGuiApplication::palette().color(QPalette::Text));
}

} // namespace OCC
