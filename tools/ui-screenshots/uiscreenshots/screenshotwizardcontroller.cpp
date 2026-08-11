/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "screenshotwizardcontroller.h"

#include "wizard/accountwizardcontroller.h"

#include <QDialog>

namespace OCC {

ScreenshotWizardController::ScreenshotWizardController(QObject *parent)
    : QObject(parent)
{
    const auto productionController = AccountWizardController{};
    _syncEverythingDescription = productionController.syncEverythingDescription();
}

void ScreenshotWizardController::setCurrentStepForScreenshot(const int step)
{
    if (_currentStep == step) {
        return;
    }
    _currentStep = step;
    emit currentStepChanged();
}

void ScreenshotWizardController::setSyncModeForScreenshot(const int syncMode)
{
    setSyncMode(syncMode);
}

void ScreenshotWizardController::submitServerUrl()
{
}

void ScreenshotWizardController::submitBasicAuth()
{
}

void ScreenshotWizardController::openBrowserLogin()
{
}

void ScreenshotWizardController::copyLoginLink()
{
}

void ScreenshotWizardController::openSignup()
{
}

void ScreenshotWizardController::openSelfHostedServerGuide()
{
}

void ScreenshotWizardController::openProxySettings()
{
    emit proxySettingsRequested();
}

void ScreenshotWizardController::cancel()
{
    emit finished(QDialog::Rejected);
}

void ScreenshotWizardController::goBack()
{
    if (_currentStep <= AccountWizardController::ServerStep) {
        return;
    }
    setCurrentStepForScreenshot(_currentStep - 1);
}

void ScreenshotWizardController::finish()
{
    emit finished(QDialog::Accepted);
}

void ScreenshotWizardController::skipFolderConfiguration()
{
    finish();
}

void ScreenshotWizardController::setSyncMode(const int syncMode)
{
    if (_syncMode == syncMode) {
        return;
    }
    _syncMode = syncMode;
    emit syncModeChanged();
}

void ScreenshotWizardController::chooseLocalSyncFolder()
{
}

void ScreenshotWizardController::openSelectiveSync()
{
    setSyncMode(AccountWizardController::SelectiveSync);
}

void ScreenshotWizardController::openAdvancedOptions()
{
    emit advancedOptionsRequested();
}

void ScreenshotWizardController::setAskBeforeLargeFolders(const bool ask)
{
    if (_askBeforeLargeFolders == ask) {
        return;
    }
    _askBeforeLargeFolders = ask;
    emit askBeforeLargeFoldersChanged();
}

void ScreenshotWizardController::setLargeFolderThresholdMb(const int thresholdMb)
{
    if (_largeFolderThresholdMb == thresholdMb) {
        return;
    }
    _largeFolderThresholdMb = thresholdMb;
    emit largeFolderThresholdMbChanged();
}

void ScreenshotWizardController::setAskBeforeExternalStorage(const bool ask)
{
    if (_askBeforeExternalStorage == ask) {
        return;
    }
    _askBeforeExternalStorage = ask;
    emit askBeforeExternalStorageChanged();
}

void ScreenshotWizardController::chooseClientCertificate()
{
    emit clientCertificateDialogRequested();
}

bool ScreenshotWizardController::submitClientCertificate()
{
    return false;
}

void ScreenshotWizardController::clearClientCertificateInput()
{
    _clientCertificatePath.clear();
    _clientCertificatePassword.clear();
    _clientCertificateError.clear();
    _clientCertificateValid = false;
    emit clientCertificateChanged();
}

void ScreenshotWizardController::retrySecureConnectionWithoutTls()
{
}

void ScreenshotWizardController::useClientCertificateForSecureConnection()
{
    emit clientCertificateDialogRequested();
}

}
