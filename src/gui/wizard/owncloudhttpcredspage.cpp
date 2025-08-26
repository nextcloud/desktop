/*
 * SPDX-FileCopyrightText: 2019 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2013 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "QProgressIndicator.h"

#include "creds/httpcredentialsgui.h"
#include "theme.h"
#include "account.h"
#include "configfile.h"
#include "wizard/owncloudhttpcredspage.h"
#include "wizard/owncloudwizardcommon.h"
#include "wizard/owncloudwizard.h"

namespace OCC {

OwncloudHttpCredsPage::OwncloudHttpCredsPage(QWidget *parent)
    : AbstractCredentialsWizardPage()
    , _ui()
    , _progressIndi(new QProgressIndicator(this))
{
    _ui.setupUi(this);

    if (parent) {
        _ocWizard = qobject_cast<OwncloudWizard *>(parent);
    }

    registerField(QLatin1String("OCUser*"), _ui.leUsername);
    registerField(QLatin1String("OCPasswd*"), _ui.lePassword);

    Theme *theme = Theme::instance();
    switch (theme->userIDType()) {
    case Theme::UserIDUserName:
        // default, handled in ui file
        break;
    case Theme::UserIDEmail:
        _ui.usernameLabel->setText(tr("&Email"));
        break;
    case Theme::UserIDCustom:
        _ui.usernameLabel->setText(theme->customUserID());
        break;
    default:
        break;
    }
    _ui.leUsername->setPlaceholderText(theme->userIDHint());

    setTitle(WizardCommon::titleTemplate().arg(tr("Connect to %1").arg(Theme::instance()->appNameGUI())));
    setSubTitle(WizardCommon::subTitleTemplate().arg(tr("Enter user credentials")));

    _ui.resultLayout->addWidget(_progressIndi);
    stopSpinner();
    setupCustomization();
}

void OwncloudHttpCredsPage::setupCustomization()
{
    // set defaults for the customize labels.
    _ui.topLabel->hide();
    _ui.bottomLabel->hide();

    Theme *theme = Theme::instance();
    QVariant variant = theme->customMedia(Theme::oCSetupTop);
    if (!variant.isNull()) {
        WizardCommon::setupCustomMedia(variant, _ui.topLabel);
    }

    variant = theme->customMedia(Theme::oCSetupBottom);
    WizardCommon::setupCustomMedia(variant, _ui.bottomLabel);
}

void OwncloudHttpCredsPage::initializePage()
{
    WizardCommon::initErrorLabel(_ui.errorLabel);

    auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
    AbstractCredentials *cred = ocWizard->account()->credentials();
    auto *httpCreds = qobject_cast<HttpCredentials *>(cred);
    if (httpCreds) {
        const QString user = httpCreds->fetchUser();
        if (!user.isEmpty()) {
            _ui.leUsername->setText(user);
        }
    } else {
        QUrl url = ocWizard->account()->url();

        // If the final url does not have a username, check the
        // user specified url too. Sometimes redirects can lose
        // the user:pw information.
        if (url.userName().isEmpty()) {
            url = ocWizard->ocUrl();
        }

        const QString user = url.userName();
        const QString password = url.password();

        if (!user.isEmpty()) {
            _ui.leUsername->setText(user);
        }
        if (!password.isEmpty()) {
            _ui.lePassword->setText(password);
        }
    }
    _ui.tokenLabel->setText(HttpCredentialsGui::requestAppPasswordText(ocWizard->account().data()));
    _ui.tokenLabel->setVisible(!_ui.tokenLabel->text().isEmpty());
    _ui.leUsername->setFocus();
}

void OwncloudHttpCredsPage::cleanupPage()
{
    _ui.leUsername->clear();
    _ui.lePassword->clear();
}

bool OwncloudHttpCredsPage::validatePage()
{
    if (_ui.leUsername->text().isEmpty() || _ui.lePassword->text().isEmpty()) {
        return false;
    }

    if (!_connected) {
        _ui.errorLabel->setVisible(false);
        startSpinner();

        // Reset cookies to ensure the username / password is actually used
        auto *ocWizard = qobject_cast<OwncloudWizard *>(wizard());
        ocWizard->account()->clearCookieJar();

        emit completeChanged();
        emit connectToOCUrl(field("OCUrl").toString().simplified());

        return false;
    } else {
        // Reset, to require another connection attempt next time
        _connected = false;

        emit completeChanged();
        stopSpinner();
        return true;
    }
    return true;
}

void OwncloudHttpCredsPage::setConnected()
{
    _connected = true;
    stopSpinner();
}

void OwncloudHttpCredsPage::startSpinner()
{
    _ui.resultLayout->setEnabled(true);
    _progressIndi->setVisible(true);
    _progressIndi->startAnimation();
}

void OwncloudHttpCredsPage::stopSpinner()
{
    _ui.resultLayout->setEnabled(false);
    _progressIndi->setVisible(false);
    _progressIndi->stopAnimation();
}

void OwncloudHttpCredsPage::setErrorString(const QString &err)
{
    if (err.isEmpty()) {
        _ui.errorLabel->setVisible(false);
    } else {
        _ui.errorLabel->setVisible(true);
        _ui.errorLabel->setText(err);
    }
    emit completeChanged();
    stopSpinner();
}

AbstractCredentials *OwncloudHttpCredsPage::getCredentials() const
{
    return new HttpCredentialsGui(_ui.leUsername->text(), _ui.lePassword->text(), _ocWizard->_clientCertBundle, _ocWizard->_clientCertPassword);
}

void OwncloudHttpCredsPage::slotStyleChanged()
{
    customizeStyle();
}

void OwncloudHttpCredsPage::customizeStyle()
{
    if(_progressIndi)
        _progressIndi->setColor(QGuiApplication::palette().color(QPalette::Text));
}

} // namespace OCC
