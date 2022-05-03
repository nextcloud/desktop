#pragma once

#include "abstractsetupwizardpage.h"

namespace Ui {
class OAuthCredentialsSetupWizardPage;
}

namespace OCC::Wizard {

class OAuthCredentialsSetupWizardPage : public AbstractSetupWizardPage
{
    Q_OBJECT

public:
    explicit OAuthCredentialsSetupWizardPage(const QUrl &serverUrl);
    void disableButtons();
    ~OAuthCredentialsSetupWizardPage() noexcept override;

    bool validateInput() override;

Q_SIGNALS:
    void openBrowserButtonPushed();
    void copyUrlToClipboardButtonPushed();

private:
    ::Ui::OAuthCredentialsSetupWizardPage *_ui;
};

} // namespace OCC::Wizard
