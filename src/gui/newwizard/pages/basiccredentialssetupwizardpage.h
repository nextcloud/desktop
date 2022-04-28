#pragma once

#include "abstractsetupwizardpage.h"

namespace Ui {
class BasicCredentialsSetupWizardPage;
}

namespace OCC::Wizard {

class BasicCredentialsSetupWizardPage : public AbstractSetupWizardPage
{
    Q_OBJECT

public:
    BasicCredentialsSetupWizardPage(const QUrl &serverUrl);
    ~BasicCredentialsSetupWizardPage() noexcept override;

    QString username() const;
    QString password() const;

    bool validateInput() override;

private:
    ::Ui::BasicCredentialsSetupWizardPage *_ui;
};

}
