/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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

    AccountPtr account = dynamic_cast<OwncloudWizard *>(wizard())->account();
    AbstractCredentials *creds = account->credentials();
    if (creds) {
        if (!creds->inherits("DummyCredentials")) {
            account->setCredentials(CredentialsFactory::create("dummy"));
        }
    }
}

int AbstractCredentialsWizardPage::nextId() const
{
    const auto ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    Q_ASSERT(ocWizard);

    if (ocWizard->needsToAcceptTermsOfService()) {
        return WizardCommon::Page_TermsOfService;
    }

    if (ocWizard->useVirtualFileSyncByDefault()) {
        return -1;
    }

    return WizardCommon::Page_AdvancedSetup;
}

}
