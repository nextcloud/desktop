/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "generalsettings.h"
#include "ui_generalsettings.h"

#include "mirall/theme.h"
#include "mirall/mirallconfigfile.h"
#include "mirall/application.h"

#include <QNetworkProxy>

namespace Mirall {

GeneralSettings::GeneralSettings(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::GeneralSettings)
{
    _ui->setupUi(this);

#if QT_VERSION >= 0x040700
    _ui->hostLineEdit->setPlaceholderText(tr("Hostname of proxy server"));
    _ui->userLineEdit->setPlaceholderText(tr("Username for proxy server"));
    _ui->passwordLineEdit->setPlaceholderText(tr("Password for proxy server"));
#endif

    _ui->typeComboBox->addItem(tr("HTTP(S) proxy"), QNetworkProxy::HttpProxy);
    _ui->typeComboBox->addItem(tr("SOCKS5 proxy"), QNetworkProxy::Socks5Proxy);

    // not implemented yet
    _ui->desktopNotificationsCheckBox->setEnabled(false);
    _ui->autostartCheckBox->setEnabled(false);

    // setup about section
    QString about = Theme::instance()->about();
    if (about.isEmpty()) {
        _ui->aboutGroupBox->hide();
    } else {
        _ui->aboutLabel->setText(about);
        _ui->aboutLabel->setOpenExternalLinks(true);
    }

    _ui->authRequiredcheckBox->setEnabled(true);

    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
            _ui->manualSettings, SLOT(setEnabled(bool)));
    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
            _ui->typeComboBox, SLOT(setEnabled(bool)));
    connect(_ui->authRequiredcheckBox, SIGNAL(toggled(bool)),
            _ui->authWidgets, SLOT(setEnabled(bool)));

    loadProxySettings();
    loadMiscSettings();

    // misc
    connect(_ui->monoIconsCheckBox, SIGNAL(toggled(bool)), SLOT(saveMiscSettings()));

    // proxy
    connect(_ui->typeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(saveProxySettings()));
    connect(_ui->proxyButtonGroup, SIGNAL(buttonClicked(int)), SLOT(saveProxySettings()));
    connect(_ui->hostLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->userLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->passwordLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->portSpinBox, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
}

GeneralSettings::~GeneralSettings()
{
    delete _ui;
}

void GeneralSettings::loadProxySettings()
{
    // load current proxy settings
    Mirall::MirallConfigFile cfgFile;
    int type = cfgFile.proxyType();
    switch (type) {
    case QNetworkProxy::NoProxy:
        _ui->noProxyRadioButton->setChecked(true);
        break;
    case QNetworkProxy::DefaultProxy:
        _ui->systemProxyRadioButton->setChecked(true);
        break;
    case QNetworkProxy::Socks5Proxy:
    case QNetworkProxy::HttpProxy:
        _ui->typeComboBox->setCurrentIndex(_ui->typeComboBox->findData(type));
        _ui->manualProxyRadioButton->setChecked(true);
        break;
    default:
        break;
    }

    _ui->hostLineEdit->setText(cfgFile.proxyHostName());
    int port = cfgFile.proxyPort();
    if (port == 0)
        port = 8080;
    _ui->portSpinBox->setValue(port);
    if (!cfgFile.proxyUser().isEmpty())
    {
        _ui->authRequiredcheckBox->setChecked(true);
        _ui->userLineEdit->setText(cfgFile.proxyUser());
        _ui->passwordLineEdit->setText(cfgFile.proxyPassword());
    }
}

void GeneralSettings::loadMiscSettings()
{
    MirallConfigFile cfgFile;
    _ui->monoIconsCheckBox->setChecked(cfgFile.monoIcons());
}

void GeneralSettings::saveMiscSettings()
{
    MirallConfigFile cfgFile;
    bool isChecked = _ui->monoIconsCheckBox->isChecked();
    cfgFile.setMonoIcons(isChecked);
    Theme::instance()->setSystrayUseMonoIcons(isChecked);
}

void GeneralSettings::saveProxySettings()
{
    MirallConfigFile cfgFile;

    if (_ui->noProxyRadioButton->isChecked()){
        cfgFile.setProxyType(QNetworkProxy::NoProxy);
    } else if (_ui->systemProxyRadioButton->isChecked()){
        cfgFile.setProxyType(QNetworkProxy::DefaultProxy);
    } else if (_ui->manualProxyRadioButton->isChecked()) {
        int type = _ui->typeComboBox->itemData(_ui->typeComboBox->currentIndex()).toInt();
        bool needsAuth = _ui->authRequiredcheckBox->isChecked();
        QString user = _ui->userLineEdit->text();
        QString pass = _ui->passwordLineEdit->text();
        cfgFile.setProxyType(type, _ui->hostLineEdit->text(),
                             _ui->portSpinBox->value(), needsAuth, user, pass);
    }

    emit proxySettingsChanged();
}

} // namespace Mirall
