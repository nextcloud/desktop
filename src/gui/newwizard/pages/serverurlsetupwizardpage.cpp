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

    // not the best style, but we hacked such branding into the pages elsewhere, too
    if (!Theme::instance()->overrideServerUrlV2().isEmpty()) {
        // note that the text should be set before the page is displayed, this way validateInput() will enable the next button
        _ui->urlLineEdit->setText(Theme::instance()->overrideServerUrlV2());

        _ui->urlLineEdit->hide();
        _ui->serverUrlLabel->hide();
    } else {
        _ui->urlLineEdit->setText(serverUrl.toString());

        connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
            _ui->urlLineEdit->setFocus();
        });
    }

    _ui->logoLabel->setText(QString());
    _ui->logoLabel->setPixmap(Theme::instance()->wizardHeaderLogo().pixmap(200, 200));

    if (!Theme::instance()->wizardUrlPostfix().isEmpty()) {
        _ui->urlLineEdit->setPostfix(Theme::instance()->wizardUrlPostfix());
    }

    connect(_ui->urlLineEdit, &QLineEdit::textChanged, this, &AbstractSetupWizardPage::contentChanged);
}

QString ServerUrlSetupWizardPage::userProvidedUrl() const
{
    QString url = _ui->urlLineEdit->text().simplified();

    if (!Theme::instance()->wizardUrlPostfix().isEmpty()) {
        url += Theme::instance()->wizardUrlPostfix();
    }

    return url;
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
