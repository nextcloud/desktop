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

#include <QDir>

#include "accountconfiguredsetupwizardstate.h"
#include "gui/folderman.h"
#include "pages/accountconfiguredwizardpage.h"
#include "theme.h"

namespace OCC::Wizard {

AccountConfiguredSetupWizardState::AccountConfiguredSetupWizardState(SetupWizardContext *context)
    : AbstractSetupWizardState(context)
{
    // being pessimistic by default
    bool vfsIsAvailable = false;
    bool enableVfsByDefault = false;
    bool vfsModeIsExperimental = false;

    switch (bestAvailableVfsMode()) {
    case Vfs::WindowsCfApi:
        vfsIsAvailable = true;
        enableVfsByDefault = true;
        vfsModeIsExperimental = false;
        break;
    case Vfs::WithSuffix:
        // we ignore forceVirtualFilesOption if experimental features are disabled
        vfsIsAvailable = Theme::instance()->enableExperimentalFeatures();
        enableVfsByDefault = false;
        vfsModeIsExperimental = true;
        break;
    default:
        break;
    }

    _page = new AccountConfiguredWizardPage(FolderMan::suggestSyncFolder(_context->accountBuilder().serverUrl(), _context->accountBuilder().displayName()), vfsIsAvailable, enableVfsByDefault, vfsModeIsExperimental);
}

SetupWizardState AccountConfiguredSetupWizardState::state() const
{
    return SetupWizardState::AccountConfiguredState;
}

void AccountConfiguredSetupWizardState::evaluatePage()
{
    auto accountConfiguredSetupWizardPage = qobject_cast<AccountConfiguredWizardPage *>(_page);
    Q_ASSERT(accountConfiguredSetupWizardPage != nullptr);

    if (accountConfiguredSetupWizardPage->syncMode() != Wizard::SyncMode::ConfigureUsingFolderWizard) {
        QString syncTargetDir = QDir::fromNativeSeparators(accountConfiguredSetupWizardPage->syncTargetDir());

        const QString errorMessageTemplate = tr("Invalid local download directory: %1");

        if (!QDir::isAbsolutePath(syncTargetDir)) {
            Q_EMIT evaluationFailed(errorMessageTemplate.arg(QStringLiteral("path must be absolute")));
            return;
        }

        QString invalidPathErrorMessage = FolderMan::checkPathValidityRecursive(syncTargetDir);
        if (!invalidPathErrorMessage.isEmpty()) {
            Q_EMIT evaluationFailed(errorMessageTemplate.arg(invalidPathErrorMessage));
            return;
        }

        _context->accountBuilder().setDefaultSyncTargetDir(syncTargetDir);
    }

    Q_EMIT evaluationSuccessful();
}

} // OCC::Wizard
