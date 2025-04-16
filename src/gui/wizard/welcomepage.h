/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QWizardPage>

#include "wizard/owncloudwizardcommon.h"

namespace OCC {

class OwncloudWizard;

namespace Ui {
    class WelcomePage;
}

class WelcomePage : public QWizardPage
{
    Q_OBJECT

public:
    explicit WelcomePage(OwncloudWizard *ocWizard);
    ~WelcomePage() override;
    [[nodiscard]] int nextId() const override;
    void initializePage() override;
    void setLoginButtonDefault();

private:
    void setupUi();
    void customizeStyle();
    void styleSlideShow();
    void setupSlideShow();
    void setupLoginButton();
    void setupCreateAccountButton();
    void setupHostYourOwnServerLabel();

    QScopedPointer<Ui::WelcomePage> _ui;

    OwncloudWizard *_ocWizard;
    WizardCommon::Pages _nextPage = WizardCommon::Page_ServerSetup;
};
}
