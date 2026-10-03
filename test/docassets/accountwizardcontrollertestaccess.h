/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "wizard/accountwizardcontroller.h"

namespace OCC
{
// Uses the controller's existing test friend without changing its production interface.
class AccountWizardControllerTestAccess
{
public:
    static void prepare(AccountWizardController &controller, AccountWizardController::Step step)
    {
        controller._initialLocalSyncFolderPromptShown = true;
        controller._localSyncFolderSelected = true;
        controller._localSyncFolder = QStringLiteral("/Users/alex/Nextcloud");
        controller._localSyncFolderValid = true;
        controller.setUserDisplayName(QStringLiteral("Alex Morgan"));
        controller.setServerDisplayName(QStringLiteral("cloud.example.com"));
        controller.setServerUrl(QStringLiteral("https://cloud.example.com"));
        controller.setBasicAuthUser(QStringLiteral("alex"));
        controller.setBasicAuthPassword(QStringLiteral("documentation-only"));
        controller.setLoginUrl(QUrl(QStringLiteral("https://cloud.example.com/login/v2/flow/documentation")));
        controller.setAuthPolling(step == AccountWizardController::BrowserAuthStep);
        controller.setNeedsSyncOptions(step == AccountWizardController::SyncOptionsStep);
        controller.setCurrentStep(step);
    }

    static bool hasLiveServices(const AccountWizardController &controller)
    {
        return hasAccount(controller) || controller._flow2Auth || controller._localSyncFolderPickerOpen;
    }

    static bool hasAccount(const AccountWizardController &controller)
    {
        return !controller._account.isNull();
    }
};
}
