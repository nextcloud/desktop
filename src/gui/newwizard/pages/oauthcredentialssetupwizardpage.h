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
    void disableReopenBrowserButton();
    ~OAuthCredentialsSetupWizardPage() noexcept override;

Q_SIGNALS:
    void reopenBrowserButtonPushed();

private:
    ::Ui::OAuthCredentialsSetupWizardPage *_ui;
};

} // namespace OCC::Wizard
