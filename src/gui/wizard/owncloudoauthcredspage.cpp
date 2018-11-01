/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
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

#include "wizard/owncloudoauthcredspage.h"
#include "theme.h"
#include "account.h"
#include "cookiejar.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "creds/httpcredentialsgui.h"
#include "creds/credentialsfactory.h"

namespace OCC {

OwncloudOAuthCredsPage::OwncloudOAuthCredsPage()
    : AbstractCredentialsWizardPage()
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
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Login in your browser")));

    connect(_ui.openLinkButton, &QCommandLinkButton::clicked, [this] {
        _ui.errorLabel->hide();
        if (_asyncAuth)
            _asyncAuth->openBrowser();
    });
    _ui.openLinkButton->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(_ui.openLinkButton, &QWidget::customContextMenuRequested, [this](const QPoint &pos) {
        auto menu = new QMenu(_ui.openLinkButton);
        menu->addAction(tr("Copy link to clipboard"), this, [this] {
            if (_asyncAuth)
                QApplication::clipboard()->setText(_asyncAuth->authorisationLink().toString(QUrl::FullyEncoded));
        });
        menu->setAttribute(Qt::WA_DeleteOnClose);
        menu->popup(_ui.openLinkButton->mapToGlobal(pos));
    });
}

void OwncloudOAuthCredsPage::initializePage()
{
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);
    ocWizard->account()->setCredentials(CredentialsFactory::create("http"));
    _asyncAuth.reset(new OAuth(ocWizard->account().data(), this));
    connect(_asyncAuth.data(), &OAuth::result, this, &OwncloudOAuthCredsPage::asyncAuthResult, Qt::QueuedConnection);
    _asyncAuth->start();
    wizard()->hide();
}

void OCC::OwncloudOAuthCredsPage::cleanupPage()
{
    // The next or back button was activated, show the wizard again
    wizard()->show();
    _asyncAuth.reset();
}

void OwncloudOAuthCredsPage::asyncAuthResult(OAuth::Result r, const QString &user,
    const QString &token, const QString &refreshToken)
{
    switch (r) {
    case OAuth::NotSupported: {
        /* OAuth not supported (can't open browser), fallback to HTTP credentials */
        OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        ocWizard->back();
        ocWizard->setAuthType(DetermineAuthTypeJob::Basic);
        break;
    }
    case OAuth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        _ui.errorLabel->show();
        wizard()->show();
        break;
    case OAuth::LoggedIn: {
        _token = token;
        _user = user;
        _refreshToken = refreshToken;
        OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        Q_ASSERT(ocWizard);
        emit connectToOCUrl(ocWizard->account()->url().toString());
        break;
    }
    }
}

int OwncloudOAuthCredsPage::nextId() const
{
    return WizardCommon::Page_AdvancedSetup;
}

void OwncloudOAuthCredsPage::setConnected()
{
    wizard()->show();
}

AbstractCredentials *OwncloudOAuthCredsPage::getCredentials() const
{
    OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);
    return new HttpCredentialsGui(_user, _token, _refreshToken,
        ocWizard->_clientSslCertificate, ocWizard->_clientSslKey);
}

bool OwncloudOAuthCredsPage::isComplete() const
{
    return false; /* We can never go forward manually */
}

} // namespace OCC
