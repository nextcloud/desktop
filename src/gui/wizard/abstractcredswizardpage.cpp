/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "creds/abstractcredentials.h"
#include "creds/credentialsfactory.h"
#include "account.h"
#include "wizard/owncloudwizard.h"

#include "wizard/abstractcredswizardpage.h"
#include <accountmanager.h>

namespace OCC {

void AbstractCredentialsWizardPage::cleanupPage()
{
    // Reset the credentials when the 'Back' button is used.

    AccountPtr account = static_cast<OwncloudWizard*>(wizard())->account();
    AbstractCredentials *creds = account->credentials();
    if (creds) {
        if (!creds->inherits("DummyCredentials")) {
            account->setCredentials(CredentialsFactory::create("dummy"));
        }
    }
}

}
