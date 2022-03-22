#include "serverurlsetupwizardpage.h"

#include "theme.h"
#include "ui_serverurlsetupwizardpage.h"

namespace OCC::Wizard {

ServerUrlSetupWizardPage::ServerUrlSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::ServerUrlSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->welcomeTextLabel->setText(tr("Welcome to %1").arg(Theme::instance()->appNameGUI()));

    _ui->urlLineEdit->setText(serverUrl.toString());

    // first, we declare this object as an event filter
    // this then allows us to make the controller the event filter of the current page
    _ui->urlLineEdit->installEventFilter(this);

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->urlLineEdit->setFocus();
    });
}

QUrl ServerUrlSetupWizardPage::serverUrl() const
{
    return QUrl::fromUserInput(_ui->urlLineEdit->text());
}

ServerUrlSetupWizardPage::~ServerUrlSetupWizardPage()
{
    delete _ui;
}
}
