/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/macOS/fileprovidersettingscontroller.h"

#include "libsync/configfile.h"

#include <QStringList>

namespace OCC::Mac {

FileProviderSettingsController *FileProviderSettingsController::instance()
{
    static FileProviderSettingsController controller;
    return &controller;
}

FileProviderSettingsController::FileProviderSettingsController(QObject *parent)
    : QObject{parent}
    , d(nullptr)
{
}

bool FileProviderSettingsController::fileProviderModeEnabled() const
{
    return ConfigFile().macFileProviderModeEnabled();
}

QStringList FileProviderSettingsController::vfsEnabledAccounts() const
{
    return {};
}

bool FileProviderSettingsController::vfsEnabledForAccount(const QString &) const
{
    return false;
}

bool FileProviderSettingsController::isOperationInProgress() const
{
    return _isOperationInProgress;
}

QString FileProviderSettingsController::operationMessage() const
{
    return _operationMessage;
}

void FileProviderSettingsController::setFileProviderModeEnabled(const bool enabled)
{
    ConfigFile().setMacFileProviderModeEnabled(enabled);
    emit fileProviderModeEnabledChanged(enabled);
    emit fileProviderModeApplyFinished(enabled, QStringList{});
}

void FileProviderSettingsController::resetVfsForAccount(const QString &)
{
}

void FileProviderSettingsController::performStartupReconciliation()
{
}

void FileProviderSettingsController::setVfsEnabledForAccount(const QString &, const bool)
{
}

void FileProviderSettingsController::applyFileProviderModeToAllAccounts(const bool)
{
}

void FileProviderSettingsController::removeAllClassicSyncFolders()
{
}

void FileProviderSettingsController::showReconciliationDialog()
{
}

QString FileProviderSettingsController::fileProviderDomainIdentifierForAccount(const QString &) const
{
    return {};
}

void FileProviderSettingsController::setOperationInProgress(const bool inProgress, const QString &message)
{
    if (_isOperationInProgress != inProgress) {
        _isOperationInProgress = inProgress;
        emit operationInProgressChanged();
    }
    if (_operationMessage != message) {
        _operationMessage = message;
        emit operationMessageChanged();
    }
}

}
