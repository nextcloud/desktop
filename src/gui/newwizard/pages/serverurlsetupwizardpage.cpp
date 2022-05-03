#include "serverurlsetupwizardpage.h"
#include "ui_serverurlsetupwizardpage.h"

#include "theme.h"

#include <QIcon>


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

    _ui->logoLabel->setText(QString());
    _ui->logoLabel->setPixmap(Theme::instance()->wizardHeaderLogo().pixmap(200, 200));
}

QString ServerUrlSetupWizardPage::userProvidedUrl() const
{
    return _ui->urlLineEdit->text();
}

ServerUrlSetupWizardPage::~ServerUrlSetupWizardPage()
{
    delete _ui;
}

bool ServerUrlSetupWizardPage::validateInput()
{
    return !_ui->urlLineEdit->text().isEmpty();
}
}
