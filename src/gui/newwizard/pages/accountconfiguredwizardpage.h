#pragma once

#include "abstractsetupwizardpage.h"

namespace Ui {
class AccountConfiguredWizardPage;
}

namespace OCC::Wizard {

class AccountConfiguredWizardPage : public AbstractSetupWizardPage
{
    Q_OBJECT

public:
    AccountConfiguredWizardPage();
    ~AccountConfiguredWizardPage() noexcept override;

private:
    ::Ui::AccountConfiguredWizardPage *_ui;
};

}
