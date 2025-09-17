/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QVariant>
#include <QVBoxLayout>

#include "flow2authcredspage.h"
#include "theme.h"
#include "account.h"
#include "cookiejar.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"
#include "wizard/flow2authwidget.h"
#include "creds/credentialsfactory.h"
#include "creds/webflowcredentials.h"

namespace OCC {

Flow2AuthCredsPage::Flow2AuthCredsPage()
    : AbstractCredentialsWizardPage()
{
    _layout = new QVBoxLayout(this);

    _flow2AuthWidget = new Flow2AuthWidget();
    _layout->addWidget(_flow2AuthWidget);

    connect(_flow2AuthWidget, &Flow2AuthWidget::authResult, this, &Flow2AuthCredsPage::slotFlow2AuthResult);

    // Connect styleChanged events to our widgets, so they can adapt (Dark-/Light-Mode switching)
    connect(this, &Flow2AuthCredsPage::styleChanged, _flow2AuthWidget, &Flow2AuthWidget::slotStyleChanged);

    // allow Flow2 page to poll on window activation
    connect(this, &Flow2AuthCredsPage::pollNow, _flow2AuthWidget, &Flow2AuthWidget::slotPollNow);
}

void Flow2AuthCredsPage::initializePage()
{
    auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);
    ocWizard->account()->setCredentials(CredentialsFactory::create("http"));

    if(_flow2AuthWidget)
        _flow2AuthWidget->startAuth(ocWizard->account().data());

    // Don't hide the wizard (avoid user confusion)!
    //wizard()->hide();

    _flow2AuthWidget->slotStyleChanged();
}

void OCC::Flow2AuthCredsPage::cleanupPage()
{
    // The next or back button was activated, show the wizard again
    wizard()->show();
    if(_flow2AuthWidget)
        _flow2AuthWidget->resetAuth();

    // Forget sensitive data
    _appPassword.clear();
    _user.clear();
}

void Flow2AuthCredsPage::slotFlow2AuthResult(Flow2Auth::Result r, const QString &errorString, const QString &user, const QString &appPassword)
{
    Q_UNUSED(errorString)
    switch (r) {
    case Flow2Auth::NotSupported: {
        /* Flow2Auth not supported (can't open browser) */
        wizard()->show();

        /* Don't fallback to HTTP credentials */
        /*OwncloudWizard *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        ocWizard->back();
        ocWizard->setAuthType(DetermineAuthTypeJob::Basic);*/
        break;
    }
    case Flow2Auth::Error:
        /* Error while getting the access token.  (Timeout, or the server did not accept our client credentials */
        wizard()->show();
        break;
    case Flow2Auth::LoggedIn: {
        _user = user;
        _appPassword = appPassword;
        auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        Q_ASSERT(ocWizard);

        emit connectToOCUrl(ocWizard->account()->url().toString());
        break;
    }
    }
}

void Flow2AuthCredsPage::setConnected()
{
    auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);

    // bring wizard to top
    ocWizard->bringToTop();
}

AbstractCredentials *Flow2AuthCredsPage::getCredentials() const
{
    auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
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

void Flow2AuthCredsPage::slotPollNow()
{
    emit pollNow();
}

void Flow2AuthCredsPage::slotStyleChanged()
{
    emit styleChanged();
}

} // namespace OCC
