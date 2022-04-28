#include "basiccredentialssetupwizardpage.h"

#include "theme.h"
#include "ui_basiccredentialssetupwizardpage.h"

namespace OCC::Wizard {

BasicCredentialsSetupWizardPage::BasicCredentialsSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::BasicCredentialsSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->urlLabel->setText(tr("Connecting to <a href='%1' style='color: %2;'>%1</a>").arg(serverUrl.toString(), Theme::instance()->wizardHeaderTitleColor().name()));

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

bool BasicCredentialsSetupWizardPage::validateInput()
{
    return !(username().isEmpty() || password().isEmpty());
}
}
