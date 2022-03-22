#include "basiccredentialssetupwizardpage.h"

#include "theme.h"
#include "ui_basiccredentialssetupwizardpage.h"

namespace OCC::Wizard {

BasicCredentialsSetupWizardPage::BasicCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::BasicCredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->urlLabel->setText(serverUrl.toString());

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->usernameLineEdit->setFocus();
    });
}

QString BasicCredentialsSetupWizardPage::username() const
{
    return _ui->usernameLineEdit->text();
}

QString BasicCredentialsSetupWizardPage::password() const
{
    return _ui->passwordLineEdit->text();
}

BasicCredentialsSetupWizardPage::~BasicCredentialsSetupWizardPage()
{
    delete _ui;
}
}
