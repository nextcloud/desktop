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
    , _afterInitialSetup(false)

{
}

void OwncloudOAuthCredsPage::setVisible(bool visible)
{
    if (!_afterInitialSetup) {
        QWizardPage::setVisible(visible);
        return;
    }

    if (isVisible() == visible) {
        return;
    }
    if (visible) {
        OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        Q_ASSERT(ocWizard);
        ocWizard->account()->setCredentials(CredentialsFactory::create("http"));
        _asyncAuth.reset(new OAuth(ocWizard->account().data(), this));
        connect(_asyncAuth.data(), SIGNAL(result(OAuth::Result, QString, QString, QString)),
            this, SLOT(asyncAuthResult(OAuth::Result, QString, QString, QString)));
        _asyncAuth->start();
        wizard()->hide();
    } else {
        // The next or back button was activated, show the wizard again
        wizard()->show();
    }
}

void OwncloudOAuthCredsPage::asyncAuthResult(OAuth::Result r, const QString &user,
    const QString &token, const QString &refreshToken)
{
    switch (r) {
    case OAuth::NotSupported:
    case OAuth::Error:
        qWarning() << "FIXME!!!";
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

void OwncloudOAuthCredsPage::initializePage()
{
    _afterInitialSetup = true;
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

} // namespace OCC
