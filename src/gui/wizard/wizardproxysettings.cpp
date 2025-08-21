/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizardproxysettings.h"

#include <QPushButton>
#include <QLoggingCategory>

namespace OCC {

Q_LOGGING_CATEGORY(lcWizardProxySettings, "nextcloud.gui.wizard.proxysettings", QtInfoMsg)

WizardProxySettings::WizardProxySettings(QUrl serverURL, QWidget *parent)
    : QDialog(parent)
    , _serverURL(std::move(serverURL))
{
    _ui.setupUi(this);

    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Proxy Settigs", "Dialog window title for proxy settings"));

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
    connect(_ui.manualProxyRadioButton, &QAbstractButton::toggled, this, &WizardProxySettings::validateProxySettings);

    connect(_ui.typeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &WizardProxySettings::validateProxySettings);
    connect(_ui.hostLineEdit, &QLineEdit::editingFinished, this, &WizardProxySettings::validateProxySettings);
    connect(_ui.userLineEdit, &QLineEdit::editingFinished, this, &WizardProxySettings::validateProxySettings);
    connect(_ui.passwordLineEdit, &QLineEdit::editingFinished, this, &WizardProxySettings::validateProxySettings);
    connect(_ui.portSpinBox, &QAbstractSpinBox::editingFinished, this, &WizardProxySettings::validateProxySettings);
    connect(_ui.authRequiredcheckBox, &QAbstractButton::toggled, this, &WizardProxySettings::validateProxySettings);

    // Warn about empty proxy host
    connect(_ui.hostLineEdit, &QLineEdit::textChanged, this, &WizardProxySettings::checkEmptyProxyHost);
    connect(_ui.hostLineEdit, &QLineEdit::textChanged, this, &WizardProxySettings::validateProxySettings);

    connect(_ui.userLineEdit, &QLineEdit::textChanged, this, &WizardProxySettings::checkEmptyProxyCredentials);
    connect(_ui.passwordLineEdit, &QLineEdit::textChanged, this, &WizardProxySettings::checkEmptyProxyCredentials);
    connect(_ui.authRequiredcheckBox, &QAbstractButton::toggled, this, &WizardProxySettings::checkEmptyProxyCredentials);

    connect(_ui.buttonBox, &QDialogButtonBox::accepted,
            this, &WizardProxySettings::settingsDone);
    connect(_ui.buttonBox, &QDialogButtonBox::rejected,
            this, &WizardProxySettings::reject);

    checkEmptyProxyHost();
    checkAccountLocalhost();

    _ui.noProxyRadioButton->setChecked(true);
    _ui.noProxyRadioButton->setFocus();
    _ui.manualSettings->setVisible(false);
}

void WizardProxySettings::checkEmptyProxyHost()
{
    if (_ui.hostLineEdit->isEnabled() && _ui.hostLineEdit->text().isEmpty()) {
        _ui.hostLineEdit->setStyleSheet("border: 1px solid red");
    } else {
        _ui.hostLineEdit->setStyleSheet(QString());
    }
}

void WizardProxySettings::checkEmptyProxyCredentials()
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

void WizardProxySettings::checkAccountLocalhost()
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

void WizardProxySettings::validateProxySettings()
{
    checkEmptyProxyHost();

    _user = _ui.userLineEdit->text();
    _password = _ui.passwordLineEdit->text();
    _host = _ui.hostLineEdit->text();
    _port = _ui.portSpinBox->value();
    _needsAuth = _ui.authRequiredcheckBox->isChecked() ? ProxyAuthentication::AuthenticationRequired : ProxyAuthentication::NoAuthentication;

    _proxyType = QNetworkProxy::NoProxy;
    _valid = false;

    if (_ui.noProxyRadioButton->isChecked()) {
        _proxyType = QNetworkProxy::NoProxy;
        _valid = true;
    } else if (_ui.systemProxyRadioButton->isChecked()) {
        _proxyType = QNetworkProxy::DefaultProxy;
        _valid = true;
    } else if (_ui.manualProxyRadioButton->isChecked()) {
        _proxyType = _ui.typeComboBox->itemData(_ui.typeComboBox->currentIndex()).value<QNetworkProxy::ProxyType>();
        _valid = true;
        if (_host.isEmpty()) {
            _proxyType = QNetworkProxy::NoProxy;
            _valid = false;
        }
        if (_ui.authRequiredcheckBox->isChecked() && (_user.isEmpty() || _password.isEmpty())) {
            _proxyType = QNetworkProxy::NoProxy;
            _valid = false;
        }
    }

    const auto okButton = _ui.buttonBox->button(QDialogButtonBox::Ok);
    if (okButton) {
        okButton->setEnabled(_valid);
    }
}

void WizardProxySettings::settingsDone()
{
    Q_EMIT proxySettingsAccepted(_user, _password, _host, _port, _needsAuth, _proxyType);
    accept();
}

}
