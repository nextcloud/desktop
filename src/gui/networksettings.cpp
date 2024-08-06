/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "networksettings.h"
#include "ui_networksettings.h"

#include "account.h"
#include "accountmanager.h"
#include "application.h"
#include "configfile.h"
#include "folderman.h"
#include "theme.h"

#include <QNetworkProxy>
#include <QString>
#include <QList>
#include <type_traits>

namespace OCC {

NetworkSettings::NetworkSettings(const AccountPtr &account, QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::NetworkSettings)
    , _account(account)
{
    _ui->setupUi(this);

    _ui->manualSettings->setVisible(_ui->manualProxyRadioButton->isChecked());

    _ui->proxyGroupBox->setVisible(!Theme::instance()->doNotUseProxy());

    if (!account) {
        _ui->globalProxySettingsRadioButton->setVisible(false);
        _ui->globalDownloadSettingsRadioButton->setVisible(false);
        _ui->globalUploadSettingsRadioButton->setVisible(false);
    }

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

    connect(_ui->globalUploadSettingsRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->uploadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->noUploadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->autoUploadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->globalDownloadSettingsRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->downloadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->noDownloadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
    connect(_ui->autoDownloadLimitRadioButton, &QAbstractButton::clicked, this, &NetworkSettings::saveBWLimitSettings);
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

    const auto useGlobalProxy = !_account || _account->networkProxySetting() == Account::AccountNetworkProxySetting::GlobalProxy;
    const auto cfgFile = ConfigFile();
    const auto proxyType = useGlobalProxy ? cfgFile.proxyType() : _account->proxyType();
    const auto proxyPort = useGlobalProxy ? cfgFile.proxyPort() : _account->proxyPort();
    const auto proxyHostName = useGlobalProxy ? cfgFile.proxyHostName() : _account->proxyHostName();
    const auto proxyNeedsAuth = useGlobalProxy ? cfgFile.proxyNeedsAuth() : _account->proxyNeedsAuth();
    const auto proxyUser = useGlobalProxy ? cfgFile.proxyUser() : _account->proxyUser();
    const auto proxyPassword = useGlobalProxy ? cfgFile.proxyPassword() : _account->proxyPassword();

    // load current proxy settings
    if (_account && _account->networkProxySetting() == Account::AccountNetworkProxySetting::GlobalProxy) {
        _ui->globalProxySettingsRadioButton->setChecked(true);
    } else {
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
    }

    _ui->hostLineEdit->setText(proxyHostName);
    _ui->portSpinBox->setValue(proxyPort == 0 ? 8080 : proxyPort);
    _ui->authRequiredcheckBox->setChecked(proxyNeedsAuth);
    _ui->userLineEdit->setText(proxyUser);
    _ui->passwordLineEdit->setText(proxyPassword);
}

void NetworkSettings::loadBWLimitSettings()
{
    const auto useGlobalLimit = !_account || _account->downloadLimitSetting() == Account::AccountNetworkTransferLimitSetting::GlobalLimit;
    const auto cfgFile = ConfigFile();
    const auto useDownloadLimit = useGlobalLimit ? cfgFile.useDownloadLimit() : static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(_account->downloadLimitSetting());
    const auto downloadLimit = useGlobalLimit ? cfgFile.downloadLimit() : _account->downloadLimit();
    const auto useUploadLimit = useGlobalLimit ? cfgFile.useUploadLimit() : static_cast<std::underlying_type_t<Account::AccountNetworkTransferLimitSetting>>(_account->uploadLimitSetting());
    const auto uploadLimit = useGlobalLimit ? cfgFile.uploadLimit() : _account->uploadLimit();

    if (_account && _account->downloadLimitSetting() == Account::AccountNetworkTransferLimitSetting::GlobalLimit) {
        _ui->globalDownloadSettingsRadioButton->setChecked(true);
    } else if (useDownloadLimit >= 1) {
        _ui->downloadLimitRadioButton->setChecked(true);
    } else if (useDownloadLimit == 0) {
        _ui->noDownloadLimitRadioButton->setChecked(true);
    } else {
        _ui->autoDownloadLimitRadioButton->setChecked(true);
    }
    _ui->downloadSpinBox->setValue(downloadLimit);

    if (_account && _account->uploadLimitSetting() == Account::AccountNetworkTransferLimitSetting::GlobalLimit) {
        _ui->globalUploadSettingsRadioButton->setChecked(true);
    } else if (useUploadLimit >= 1) {
        _ui->uploadLimitRadioButton->setChecked(true);
    } else if (useUploadLimit == 0) {
        _ui->noUploadLimitRadioButton->setChecked(true);
    } else {
        _ui->autoUploadLimitRadioButton->setChecked(true);
    }
    _ui->uploadSpinBox->setValue(uploadLimit);
}

void NetworkSettings::saveProxySettings()
{
    checkEmptyProxyHost();

    const auto useGlobalProxy = _ui->globalProxySettingsRadioButton->isChecked();
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
        const auto proxySetting = useGlobalProxy ? Account::AccountNetworkProxySetting::GlobalProxy : Account::AccountNetworkProxySetting::AccountSpecificProxy;
        _account->setProxySettings(proxySetting, proxyType, host, port, needsAuth, user, password);
        const auto accountState = AccountManager::instance()->accountFromUserId(_account->userIdAtHostWithPort());
        accountState->freshConnectionAttempt();
        AccountManager::instance()->saveAccount(_account.data());
    } else {
        ConfigFile().setProxyType(proxyType, host, port, needsAuth, user, password);
        ClientProxy proxy;
        proxy.setupQtProxyFromConfig(); // Refresh the Qt proxy settings as the
        // quota check can happen all the time.

        // ...and set the folders dirty, they refresh their proxy next time they
        // start the sync.
        FolderMan::instance()->setDirtyProxy();

        const auto accounts = AccountManager::instance()->accounts();
        for (const auto &accountState : accounts) {
            if (accountState->account()->networkProxySetting() == Account::AccountNetworkProxySetting::GlobalProxy) {
                accountState->freshConnectionAttempt();
            }
        }
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
    } else if (_ui->autoDownloadLimitRadioButton->isChecked()) {
        useDownloadLimit = -1;
    } else if (_account && _ui->globalDownloadSettingsRadioButton->isChecked()) {
        useDownloadLimit = -2;
    }

    if (_ui->uploadLimitRadioButton->isChecked()) {
        useUploadLimit = 1;
    } else if (_ui->noUploadLimitRadioButton->isChecked()) {
        useUploadLimit = 0;
    } else if (_ui->autoUploadLimitRadioButton->isChecked()) {
        useUploadLimit = -1;
    } else if (_account && _ui->globalUploadSettingsRadioButton->isChecked()) {
        useUploadLimit = -2;
    }

    if (_account) {
        _account->setDownloadLimitSetting(static_cast<Account::AccountNetworkTransferLimitSetting>(useDownloadLimit));
        _account->setDownloadLimit(downloadLimit);
        _account->setUploadLimitSetting(static_cast<Account::AccountNetworkTransferLimitSetting>(useUploadLimit));
        _account->setUploadLimit(uploadLimit);
        AccountManager::instance()->saveAccount(_account.data());
    } else {
        ConfigFile cfg;
        cfg.setUseDownloadLimit(useDownloadLimit);
        cfg.setUseUploadLimit(useUploadLimit);
        cfg.setDownloadLimit(downloadLimit);
        cfg.setUploadLimit(uploadLimit);
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
