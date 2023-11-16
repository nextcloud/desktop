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
#include "libsync/filesystem.h"
#include "pages/accountconfiguredwizardpage.h"
#include "theme.h"

namespace OCC::Wizard {

AccountConfiguredSetupWizardState::AccountConfiguredSetupWizardState(SetupWizardContext *context)
    : AbstractSetupWizardState(context)
{
    // being pessimistic by default
    bool vfsIsAvailable = false;
    bool enableVfsByDefault = false;

    switch (VfsPluginManager::instance().bestAvailableVfsMode()) {
    case Vfs::WindowsCfApi:
        vfsIsAvailable = true;
        enableVfsByDefault = true;
        break;
    case Vfs::WithSuffix:
        // we ignore forceVirtualFilesOption if experimental features are disabled
        vfsIsAvailable = false;
        enableVfsByDefault = false;
        break;
    default:
        break;
    }

    const auto urlToSuggestSyncFolderFor = [this]() {
        const auto selectedInstance = _context->accountBuilder().webFingerSelectedInstance();

        if (!selectedInstance.isEmpty()) {
            return selectedInstance;
        }

        return _context->accountBuilder().serverUrl();
    }();

    QString defaultSyncTargetDir = FolderMan::suggestSyncFolder(urlToSuggestSyncFolderFor, _context->accountBuilder().displayName());
    QString syncTargetDir = _context->accountBuilder().syncTargetDir();

    if (syncTargetDir.isEmpty()) {
        syncTargetDir = defaultSyncTargetDir;
    }

    _page = new AccountConfiguredWizardPage(defaultSyncTargetDir, syncTargetDir, vfsIsAvailable, enableVfsByDefault);
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

        // make sure we remember it now so we can show it to the user again upon failures
        _context->accountBuilder().setSyncTargetDir(syncTargetDir);

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
    }

    Q_EMIT evaluationSuccessful();
}

} // OCC::Wizard
