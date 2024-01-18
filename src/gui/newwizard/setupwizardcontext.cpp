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

#include "setupwizardcontext.h"
#include "gui/settingsdialog.h"

namespace OCC::Wizard {

SetupWizardContext::SetupWizardContext(SettingsDialog *windowParent, QObject *parent)
    : QObject(parent)
    , _window(new SetupWizardWindow(windowParent))
{
    resetAccessManager();
}

SetupWizardContext::~SetupWizardContext()
{
    // the widget might already have been deleted by the destuction of its parent widget
    if (_window) {
        _window->deleteLater();
    }
    _accessManager->deleteLater();
}

AccessManager *SetupWizardContext::resetAccessManager()
{
    if (_accessManager != nullptr) {
        _accessManager->deleteLater();
    }

    _accessManager = new AccessManager(this);
    return _accessManager;
}

SetupWizardWindow *SetupWizardContext::window() const
{
    return _window;
}

SetupWizardAccountBuilder &SetupWizardContext::accountBuilder()
{
    return _accountBuilder;
}

AccessManager *SetupWizardContext::accessManager() const
{
    return _accessManager;
}

void SetupWizardContext::resetAccountBuilder()
{
    _accountBuilder = {};
}

CoreJob *SetupWizardContext::startFetchUserInfoJob(QObject *parent) const
{
    const QUrl serverUrl = [this]() {
        const QUrl webFingerInstance = _accountBuilder.webFingerSelectedInstance();
        if (!webFingerInstance.isEmpty()) {
            return webFingerInstance;
        } else {
            return _accountBuilder.serverUrl();
        }
    }();

    return _accountBuilder.authenticationStrategy()->makeFetchUserInfoJobFactory(_accessManager).startJob(serverUrl, parent);
}

} // OCC::Wizard
