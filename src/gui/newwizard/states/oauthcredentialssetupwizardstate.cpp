/*
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include <QApplication>
#include <QClipboard>

#include "gui/application.h"
#include "oauthcredentialssetupwizardstate.h"

namespace OCC::Wizard {

OAuthCredentialsSetupWizardState::OAuthCredentialsSetupWizardState(SetupWizardContext *context)
    : AbstractSetupWizardState(context)
{
    auto oAuthCredentialsPage = new OAuthCredentialsSetupWizardPage(_context->accountBuilder().serverUrl());
    _page = oAuthCredentialsPage;

    // username might not be set yet, shouldn't matter, though
    auto oAuth = new OAuth(_context->accountBuilder().serverUrl(), QString(), _context->accessManager(), {}, this);

    connect(oAuth, &OAuth::result, this, [this, oAuthCredentialsPage](OAuth::Result result, const QString &userName, const QString &token, const QString &displayName, const QString &refreshToken) {
        // the button may not be clicked anymore, since the server has been shut down right before this signal was emitted by the OAuth instance
        oAuthCredentialsPage->disableButtons();

        _context->window()->slotStartTransition();

        // bring window up top again, as the browser may have been raised in front of it
        _context->window()->raise();

        switch (result) {
        case OAuth::Result::LoggedIn: {
            _context->accountBuilder().setAuthenticationStrategy(new OAuth2AuthenticationStrategy(userName, token, refreshToken));
            _context->accountBuilder().setDisplayName(displayName);
            Q_EMIT evaluationSuccessful();
            break;
        }
        case OAuth::Result::Error: {
            Q_EMIT evaluationFailed(tr("Error while trying to log in to OAuth2-enabled server."));
            break;
        }
        case OAuth::Result::NotSupported: {
            // should never happen
            Q_EMIT evaluationFailed(tr("Server reports that OAuth2 is not supported."));
            break;
        }
        }
    });

    connect(oAuthCredentialsPage, &OAuthCredentialsSetupWizardPage::openBrowserButtonPushed, this, [oAuth]() {
        oAuth->openBrowser();
    });

    oAuthCredentialsPage->setButtonsEnabled(false);
    connect(oAuth, &OAuth::authorisationLinkChanged, this, [oAuthCredentialsPage]() {
        oAuthCredentialsPage->setButtonsEnabled(true);
    });

    connect(oAuthCredentialsPage, &OAuthCredentialsSetupWizardPage::copyUrlToClipboardButtonPushed, this, [oAuth]() {
        const auto link = oAuth->authorisationLink();
        Q_ASSERT(!link.isEmpty());
        ocApp()->clipboard()->setText(link.toString());
    });

    // moving to next page is only possible once we see a request to our embedded web server
    _context->window()->disableNextButton();

    oAuth->startAuthentication();
}

SetupWizardState OAuthCredentialsSetupWizardState::state() const
{
    return SetupWizardState::CredentialsState;
}

void OAuthCredentialsSetupWizardState::evaluatePage()
{
    // the next button is disabled anyway, since moving forward is controlled by the OAuth object signal handlers
    // therefore, this method should never ever be called
    Q_UNREACHABLE();
}

} // OCC::Wizard
