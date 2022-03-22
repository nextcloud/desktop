#include "oauthcredentialssetupwizardpage.h"

#include "theme.h"
#include "ui_oauthcredentialssetupwizardpage.h"

namespace OCC::Wizard {

OAuthCredentialsSetupWizardPage::OAuthCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::OAuthCredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->urlLabel->setText(serverUrl.toString());

    connect(_ui->reopenBrowserButton, &QPushButton::pressed, this, [this]() {
        Q_EMIT reopenBrowserButtonPushed();
    });

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->reopenBrowserButton->setFocus();
    });
}

void OAuthCredentialsSetupWizardPage::disableReopenBrowserButton()
{
    _ui->reopenBrowserButton->setEnabled(false);
}

OAuthCredentialsSetupWizardPage::~OAuthCredentialsSetupWizardPage()
{
    delete _ui;
}

} // namespace OCC::Wizard
