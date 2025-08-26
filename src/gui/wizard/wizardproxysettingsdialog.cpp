/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizardproxysettingsdialog.h"

#include <QPushButton>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardProxySettings, "nextcloud.gui.wizard.proxysettings", QtInfoMsg)

WizardProxySettingsDialog::WizardProxySettingsDialog(QUrl serverURL,
                                                     WizardProxySettings proxySettings,
                                                     QWidget *parent)
    : QDialog(parent)
{
    _ui.setupUi(this);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Proxy Settings", "Dialog window title for proxy settings"));

    _ui.hostLineEdit->setPlaceholderText(tr("Hostname of proxy server"));
    _ui.userLineEdit->setPlaceholderText(tr("Username for proxy server"));
    _ui.passwordLineEdit->setPlaceholderText(tr("Password for proxy server"));

    _ui.typeComboBox->addItem(tr("HTTP(S) proxy"), QNetworkProxy::HttpProxy);
    _ui.typeComboBox->addItem(tr("SOCKS5 proxy"), QNetworkProxy::Socks5Proxy);

    _ui.authRequiredcheckBox->setEnabled(true);

    // Explicitly set up the enabled status of the proxy auth widgets to ensure
    // toggling the parent enables/disables the children
    _ui.userLineEdit->setEnabled(true);
    _ui.passwordLineEdit->setEnabled(true);
    _ui.authWidgets->setEnabled(_ui.authRequiredcheckBox->isChecked());
    connect(_ui.authRequiredcheckBox, &QAbstractButton::toggled, _ui.authWidgets, &QWidget::setEnabled);

    connect(_ui.manualProxyRadioButton, &QAbstractButton::toggled, _ui.manualSettings, &QWidget::setVisible);
    connect(_ui.manualProxyRadioButton, &QAbstractButton::toggled, this, &WizardProxySettingsDialog::validateProxySettings);

    connect(_ui.typeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &WizardProxySettingsDialog::validateProxySettings);
    connect(_ui.authRequiredcheckBox, &QAbstractButton::toggled, this, &WizardProxySettingsDialog::validateProxySettings);

    // Warn about empty proxy host
    connect(_ui.hostLineEdit, &QLineEdit::textChanged, this, &WizardProxySettingsDialog::validateProxySettings);
    connect(_ui.userLineEdit, &QLineEdit::textChanged, this, &WizardProxySettingsDialog::validateProxySettings);
    connect(_ui.passwordLineEdit, &QLineEdit::textChanged, this, &WizardProxySettingsDialog::validateProxySettings);
    connect(_ui.portSpinBox, &QSpinBox::valueChanged, this, &WizardProxySettingsDialog::validateProxySettings);
    connect(_ui.authRequiredcheckBox, &QAbstractButton::toggled, this, &WizardProxySettingsDialog::validateProxySettings);

    connect(_ui.buttonBox, &QDialogButtonBox::accepted,
            this, &WizardProxySettingsDialog::settingsDone);
    connect(_ui.buttonBox, &QDialogButtonBox::rejected,
            this, &WizardProxySettingsDialog::reject);

    setServerUrl(std::move(serverURL));
    setProxySettings(std::move(proxySettings));
}

void WizardProxySettingsDialog::setServerUrl(QUrl serverUrl)
{
    if (_serverURL == serverUrl) {
        return;
    }

    _serverURL = std::move(serverUrl);
    checkAccountLocalhost();
}

void WizardProxySettingsDialog::setProxySettings(WizardProxySettings proxySettings)
{
    if (_settings == proxySettings) {
        return;
    }

    _settings = std::move(proxySettings);

    if (!_settings._user.isEmpty()) {
        _ui.userLineEdit->setText(_settings._user);
    }
    if (!_settings._password.isEmpty()) {
        _ui.passwordLineEdit->setText(_settings._password);
    }
    if (!_settings._host.isEmpty()) {
        _ui.hostLineEdit->setText(_settings._host);
    }

    _ui.authRequiredcheckBox->setChecked(_settings._needsAuth != ProxyAuthentication::NoAuthentication);

    switch (_settings._proxyType)
    {
    case QNetworkProxy::NoProxy:
        _ui.noProxyRadioButton->setChecked(true);
        _ui.noProxyRadioButton->setFocus();
        _ui.manualSettings->setVisible(false);
        break;
    case QNetworkProxy::ProxyType::DefaultProxy:
        _ui.systemProxyRadioButton->setChecked(true);
        _ui.systemProxyRadioButton->setFocus();
        _ui.manualSettings->setVisible(false);
        break;
    case QNetworkProxy::Socks5Proxy:
    case QNetworkProxy::HttpProxy:
        _ui.manualProxyRadioButton->setChecked(true);
        _ui.manualProxyRadioButton->setFocus();
        _ui.manualSettings->setVisible(true);
        break;
    case QNetworkProxy::HttpCachingProxy:
    case QNetworkProxy::FtpCachingProxy:
        break;
    }

    validateProxySettings();
}

void WizardProxySettingsDialog::checkEmptyProxyHost()
{
    if (_ui.hostLineEdit->isEnabled() && _ui.hostLineEdit->text().isEmpty()) {
        _ui.hostLineEdit->setStyleSheet("border: 1px solid red");
    } else {
        _ui.hostLineEdit->setStyleSheet(QString());
    }
}

void WizardProxySettingsDialog::checkEmptyProxyCredentials()
{
    if (!_ui.authRequiredcheckBox->isChecked()) {
        _ui.userLineEdit->setStyleSheet(QString());
        _ui.passwordLineEdit->setStyleSheet(QString());
        return;
    }

    if (_ui.userLineEdit->text().isEmpty()) {
        _ui.userLineEdit->setStyleSheet("border: 1px solid red");
   } else {
       _ui.userLineEdit->setStyleSheet(QString());
   }

   if (_ui.passwordLineEdit->text().isEmpty()) {
       _ui.passwordLineEdit->setStyleSheet("border: 1px solid red");
   } else {
       _ui.passwordLineEdit->setStyleSheet(QString());
   }
}

void WizardProxySettingsDialog::checkAccountLocalhost()
{
    auto visible = false;
    if (_ui.manualProxyRadioButton->isChecked()) {
        const auto host = _serverURL.host();
        // Some typical url for localhost
        if (host == "localhost" || host.startsWith("127.") || host == "[::1]") {
            visible = true;
        }
    }
    _ui.labelLocalhost->setVisible(visible);
}

void WizardProxySettingsDialog::validateProxySettings()
{
    checkEmptyProxyHost();
    checkEmptyProxyCredentials();
    checkAccountLocalhost();

    _settings._user = _ui.userLineEdit->text();
    _settings._password = _ui.passwordLineEdit->text();
    _settings._host = _ui.hostLineEdit->text();
    _settings._port = _ui.portSpinBox->value();
    _settings._needsAuth = _ui.authRequiredcheckBox->isChecked() ? ProxyAuthentication::AuthenticationRequired : ProxyAuthentication::NoAuthentication;

    _settings._proxyType = QNetworkProxy::NoProxy;
    _valid = false;

    if (_ui.noProxyRadioButton->isChecked()) {
        _settings._proxyType = QNetworkProxy::NoProxy;
        _valid = true;
    } else if (_ui.systemProxyRadioButton->isChecked()) {
        _settings._proxyType = QNetworkProxy::DefaultProxy;
        _valid = true;
    } else if (_ui.manualProxyRadioButton->isChecked()) {
        _settings._proxyType = _ui.typeComboBox->itemData(_ui.typeComboBox->currentIndex()).value<QNetworkProxy::ProxyType>();
        _valid = true;
        if (_settings._host.isEmpty()) {
            _settings._proxyType = QNetworkProxy::NoProxy;
            _valid = false;
        }
        if (_ui.authRequiredcheckBox->isChecked() && (_settings._user.isEmpty() || _settings._password.isEmpty())) {
            _settings._proxyType = QNetworkProxy::NoProxy;
            _valid = false;
        }
    }

    const auto okButton = _ui.buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setEnabled(_valid);
    }
}

void WizardProxySettingsDialog::settingsDone()
{
    Q_EMIT proxySettingsAccepted(_settings);
    accept();
}

}
