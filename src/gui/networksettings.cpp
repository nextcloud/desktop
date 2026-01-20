/*
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "networksettings.h"
#include "ui_networksettings.h"

#include "account.h"
#include "accountmanager.h"
#include "application.h"
#include "configfile.h"
#include "folderman.h"
#include "theme.h"

#include <QShowEvent>
#include <QNetworkProxy>
#include <QString>
#include <QList>
#include <QPalette>
#include <type_traits>

namespace OCC {

NetworkSettings::NetworkSettings(const AccountPtr &account, QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::NetworkSettings)
    , _account(account)
{
    _ui->setupUi(this);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::AlternateBase);
    _ui->proxyGroupBox->setAutoFillBackground(true);
    _ui->proxyGroupBox->setBackgroundRole(QPalette::AlternateBase);
    _ui->downloadBox->setAutoFillBackground(true);
    _ui->downloadBox->setBackgroundRole(QPalette::AlternateBase);
    _ui->uploadBox->setAutoFillBackground(true);
    _ui->uploadBox->setBackgroundRole(QPalette::AlternateBase);

    _ui->manualSettings->setVisible(_ui->manualProxyRadioButton->isChecked());

    _ui->proxyGroupBox->setVisible(!Theme::instance()->doNotUseProxy());

    if (!Theme::instance()->doNotUseProxy()) {
        _ui->hostLineEdit->setPlaceholderText(tr("Hostname of proxy server"));
        _ui->userLineEdit->setPlaceholderText(tr("Username for proxy server"));
        _ui->passwordLineEdit->setPlaceholderText(tr("Password for proxy server"));

        _ui->typeComboBox->addItem(tr("HTTP(S) proxy"), QNetworkProxy::HttpProxy);
        _ui->typeComboBox->addItem(tr("SOCKS5 proxy"), QNetworkProxy::Socks5Proxy);

        _ui->authRequiredcheckBox->setEnabled(true);

        // Explicitly set up the enabled status of the proxy auth widgets to ensure
        // toggling the parent enables/disables the children
        _ui->userLineEdit->setEnabled(true);
        _ui->passwordLineEdit->setEnabled(true);
        _ui->authWidgets->setEnabled(_ui->authRequiredcheckBox->isChecked());
        connect(_ui->authRequiredcheckBox, &QAbstractButton::toggled, _ui->authWidgets, &QWidget::setEnabled);

        connect(_ui->manualProxyRadioButton, &QAbstractButton::toggled, _ui->manualSettings, &QWidget::setVisible);
        connect(_ui->manualProxyRadioButton, &QAbstractButton::toggled, this, &NetworkSettings::checkAccountLocalhost);

        loadProxySettings();

        connect(_ui->typeComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &NetworkSettings::saveProxySettings);
        connect(_ui->proxyButtonGroup, &QButtonGroup::buttonClicked, this, &NetworkSettings::saveProxySettings);
        connect(_ui->hostLineEdit, &QLineEdit::editingFinished, this, &NetworkSettings::saveProxySettings);
        connect(_ui->userLineEdit, &QLineEdit::editingFinished, this, &NetworkSettings::saveProxySettings);
        connect(_ui->passwordLineEdit, &QLineEdit::editingFinished, this, &NetworkSettings::saveProxySettings);
        connect(_ui->portSpinBox, &QAbstractSpinBox::editingFinished, this, &NetworkSettings::saveProxySettings);
        connect(_ui->authRequiredcheckBox, &QAbstractButton::toggled, this, &NetworkSettings::saveProxySettings);

        // Warn about empty proxy host
        connect(_ui->hostLineEdit, &QLineEdit::textChanged, this, &NetworkSettings::checkEmptyProxyHost);
        checkEmptyProxyHost();
        checkAccountLocalhost();
    } else {
        _ui->noProxyRadioButton->setChecked(false);
    }

    loadBWLimitSettings();

    _ui->downloadSpinBox->setVisible(_ui->downloadLimitRadioButton->isChecked());
    _ui->downloadSpinBoxLabel->setVisible(_ui->downloadLimitRadioButton->isChecked());
    _ui->uploadSpinBox->setVisible(_ui->uploadLimitRadioButton->isChecked());
    _ui->uploadSpinBoxLabel->setVisible(_ui->uploadLimitRadioButton->isChecked());

    connect(_ui->uploadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->noUploadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->downloadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->noDownloadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->downloadSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->uploadSpinBox, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), this, &NetworkSettings::saveBWLimitSettings);
}

NetworkSettings::~NetworkSettings()
{
    delete _ui;
}

QSize NetworkSettings::sizeHint() const
{
    return {
        ownCloudGui::settingsDialogSize().width(),
        QWidget::sizeHint().height()
    };
}

void NetworkSettings::loadProxySettings()
{
    if (Theme::instance()->forceSystemNetworkProxy()) {
        _ui->systemProxyRadioButton->setChecked(true);
        _ui->proxyGroupBox->setEnabled(false);
        return;
    }

    const auto proxyType = _account->proxyType();
    const auto proxyPort = _account->proxyPort();
    const auto proxyHostName = _account->proxyHostName();
    const auto proxyNeedsAuth = _account->proxyNeedsAuth();
    const auto proxyUser = _account->proxyUser();
    const auto proxyPassword = _account->proxyPassword();

    // load current proxy settings
    switch (proxyType) {
    case QNetworkProxy::NoProxy:
        _ui->noProxyRadioButton->setChecked(true);
        break;
    case QNetworkProxy::DefaultProxy:
        _ui->systemProxyRadioButton->setChecked(true);
        break;
    case QNetworkProxy::Socks5Proxy:
    case QNetworkProxy::HttpProxy:
        _ui->typeComboBox->setCurrentIndex(_ui->typeComboBox->findData(proxyType));
        _ui->manualProxyRadioButton->setChecked(true);
        break;
    default:
        break;
    }

    _ui->hostLineEdit->setText(proxyHostName);
    _ui->portSpinBox->setValue(proxyPort == 0 ? 8080 : proxyPort);
    _ui->authRequiredcheckBox->setChecked(proxyNeedsAuth);
    _ui->userLineEdit->setText(proxyUser);
    _ui->passwordLineEdit->setText(proxyPassword);
}

void NetworkSettings::loadBWLimitSettings()
{
    const auto useDownloadLimit = static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(_account->downloadLimitSetting());
    const auto downloadLimit = _account->downloadLimit();
    const auto useUploadLimit = static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(_account->uploadLimitSetting());
    const auto uploadLimit = _account->uploadLimit();

    if (useDownloadLimit >= 1) {
        _ui->downloadLimitRadioButton->setChecked(true);
    } else {
        _ui->noDownloadLimitRadioButton->setChecked(true);
    }
    _ui->downloadSpinBox->setValue(downloadLimit);

    if (useUploadLimit >= 1) {
        _ui->uploadLimitRadioButton->setChecked(true);
    } else {
        _ui->noUploadLimitRadioButton->setChecked(true);
    }
    _ui->uploadSpinBox->setValue(uploadLimit);
}

void NetworkSettings::saveProxySettings()
{
    checkEmptyProxyHost();

    const auto user = _ui->userLineEdit->text();
    const auto password = _ui->passwordLineEdit->text();
    const auto host = _ui->hostLineEdit->text();
    const auto port = _ui->portSpinBox->value();
    const auto needsAuth = _ui->authRequiredcheckBox->isChecked();

    auto proxyType = QNetworkProxy::NoProxy;

    if (_ui->noProxyRadioButton->isChecked()) {
        proxyType = QNetworkProxy::NoProxy;
    } else if (_ui->systemProxyRadioButton->isChecked()) {
        proxyType = QNetworkProxy::DefaultProxy;
    } else if (_ui->manualProxyRadioButton->isChecked()) {
        proxyType = _ui->typeComboBox->itemData(_ui->typeComboBox->currentIndex()).value<QNetworkProxy::ProxyType>();
        if (host.isEmpty()) {
            proxyType = QNetworkProxy::NoProxy;
        }
    }

    if (_account) { // We must be setting up network proxy for a specific account
        _account->setProxySettings(proxyType, host, port, needsAuth, user, password);
        const auto accountState = AccountManager::instance()->accountFromUserId(_account->userIdAtHostWithPort());
        if (accountState) {
            accountState->freshConnectionAttempt();
        }
        AccountManager::instance()->saveAccount(_account);
    }
}

void NetworkSettings::saveBWLimitSettings()
{
    const auto downloadLimit = _ui->downloadSpinBox->value();
    const auto uploadLimit = _ui->uploadSpinBox->value();

    auto useDownloadLimit = 0;
    auto useUploadLimit = 0;

    if (_ui->downloadLimitRadioButton->isChecked()) {
        useDownloadLimit = 1;
    } else if (_ui->noDownloadLimitRadioButton->isChecked()) {
        useDownloadLimit = 0;
    } else if (_account) {
        useDownloadLimit = -2;
    }

    if (_ui->uploadLimitRadioButton->isChecked()) {
        useUploadLimit = 1;
    } else if (_ui->noUploadLimitRadioButton->isChecked()) {
        useUploadLimit = 0;
    } else if (_account) {
        useUploadLimit = -2;
    }

    if (_account) {
        _account->setDownloadLimitSetting(static_cast<Account::AccountNetworkTransferLimitSetting>(useDownloadLimit));
        _account->setDownloadLimit(downloadLimit);
        _account->setUploadLimitSetting(static_cast<Account::AccountNetworkTransferLimitSetting>(useUploadLimit));
        _account->setUploadLimit(uploadLimit);
        AccountManager::instance()->saveAccount(_account);
    }

    FolderMan::instance()->setDirtyNetworkLimits(_account);
}

void NetworkSettings::checkEmptyProxyHost()
{
    if (_ui->hostLineEdit->isEnabled() && _ui->hostLineEdit->text().isEmpty()) {
        _ui->hostLineEdit->setStyleSheet("border: 1px solid red");
    } else {
        _ui->hostLineEdit->setStyleSheet(QString());
    }
}

void NetworkSettings::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()
        && _ui->manualProxyRadioButton->isChecked()
        && _ui->hostLineEdit->text().isEmpty()) {
        _ui->noProxyRadioButton->setChecked(true);
        checkEmptyProxyHost();
        saveProxySettings();
    }
    checkAccountLocalhost();

    QWidget::showEvent(event);
}


void NetworkSettings::checkAccountLocalhost()
{
    bool visible = false;
    if (_ui->manualProxyRadioButton->isChecked()) {
        // Check if at least one account is using localhost, because Qt proxy settings have no
        // effect for localhost (#7169)
        const auto accounts = AccountManager::instance()->accounts();
        for (const auto &account : accounts) {
            const auto host = account->account()->url().host();
            // Some typical url for localhost
            if (host == "localhost" || host.startsWith("127.") || host == "[::1]")
                visible = true;
        }
    }
    _ui->labelLocalhost->setVisible(visible);
}


} // namespace OCC
