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

#include "abstractsetupwizardpage.h"
#include "setupwizardcontroller.h"

#include <QSharedPointer>

namespace Ui {
class AccountConfiguredWizardPage;
}

namespace OCC::Wizard {

class AccountConfiguredWizardPage : public AbstractSetupWizardPage
{
    Q_OBJECT

public:
    explicit AccountConfiguredWizardPage(
        const QString &defaultSyncTargetDir, const QString &userChosenSyncTargetDir, bool vfsIsAvailable, bool enableVfsByDefault, bool vfsModeIsExperimental);
    ~AccountConfiguredWizardPage() noexcept override;

    QString syncTargetDir() const;

    SyncMode syncMode() const;

    bool validateInput() override;

    void setShowAdvancedSettings(bool showAdvancedSettings);

private:
    ::Ui::AccountConfiguredWizardPage *_ui;
};

}
