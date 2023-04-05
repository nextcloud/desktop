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

#pragma once

#include "accessmanager.h"
#include "setupwizardwindow.h"

#include <QtGlobal>

namespace OCC {
class SettingsDialog;
}

namespace OCC::Wizard {

/**
 * This class makes sharing wizard related objects between the controller and the states easier.
 * It also allows us to provide standardized
 */
class SetupWizardContext : public QObject
{
    Q_OBJECT

    Q_DISABLE_COPY_MOVE(SetupWizardContext)

public:
    explicit SetupWizardContext(SettingsDialog *windowParent, QObject *parent);
    ~SetupWizardContext() override;

    /**
     * Delete old access manager and create a new one.
     * @return
     */
    AccessManager *resetAccessManager();

    SetupWizardWindow *window() const;

    SetupWizardAccountBuilder &accountBuilder();
    void resetAccountBuilder();

    AccessManager *accessManager() const;

    // convenience factory
    CoreJob *startFetchUserInfoJob(QObject *parent) const;

private:
    SetupWizardWindow *_window = nullptr;
    AccessManager *_accessManager = nullptr;
    SetupWizardAccountBuilder _accountBuilder;
};

} // OCC::Wizard
