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

#include "basiccredentialssetupwizardstate.h"
#include "networkjobs/fetchuserinfojobfactory.h"

namespace OCC::Wizard {

BasicCredentialsSetupWizardState::BasicCredentialsSetupWizardState(SetupWizardContext *context)
    : AbstractSetupWizardState(context)
{
    if (!context->accountBuilder().legacyWebFingerUsername().isEmpty()) {
        _page =
            BasicCredentialsSetupWizardPage::createForWebFinger(_context->accountBuilder().serverUrl(), _context->accountBuilder().legacyWebFingerUsername());
    } else {
        _page = new BasicCredentialsSetupWizardPage(_context->accountBuilder().serverUrl());
    }
}

void BasicCredentialsSetupWizardState::evaluatePage()
{
    auto *basicCredentialsSetupWizardPage = qobject_cast<BasicCredentialsSetupWizardPage *>(_page);
    Q_ASSERT(basicCredentialsSetupWizardPage != nullptr);

    const QString username = basicCredentialsSetupWizardPage->username();
    const QString password = basicCredentialsSetupWizardPage->password();

    _context->accountBuilder().setAuthenticationStrategy(new HttpBasicAuthenticationStrategy(username, password));

    if (!_context->accountBuilder().hasValidCredentials()) {
        Q_EMIT evaluationFailed(tr("Invalid credentials"));
    }
    Q_EMIT evaluationSuccessful();
}

SetupWizardState BasicCredentialsSetupWizardState::state() const
{
    return SetupWizardState::CredentialsState;
}

} // OCC::Wizard
