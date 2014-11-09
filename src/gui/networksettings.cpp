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

#include "networksettings.h"
#include "ui_networksettings.h"

#include "theme.h"
#include "configfile.h"
#include "application.h"
#include "utility.h"
#include "configfile.h"
#include "folderman.h"

#include <QNetworkProxy>

namespace OCC {

NetworkSettings::NetworkSettings(QWidget *parent) :
    QWidget(parent),
    _ui(new Ui::NetworkSettings)
{
    _ui->setupUi(this);

    _ui->hostLineEdit->setPlaceholderText(tr("Hostname of proxy server"));
    _ui->userLineEdit->setPlaceholderText(tr("Username for proxy server"));
    _ui->passwordLineEdit->setPlaceholderText(tr("Password for proxy server"));

    _ui->typeComboBox->addItem(tr("HTTP(S) proxy"), QNetworkProxy::HttpProxy);
    _ui->typeComboBox->addItem(tr("SOCKS5 proxy"), QNetworkProxy::Socks5Proxy);

    _ui->authRequiredcheckBox->setEnabled(true);

    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
            _ui->manualSettings, SLOT(setEnabled(bool)));
    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
            _ui->typeComboBox, SLOT(setEnabled(bool)));
    connect(_ui->authRequiredcheckBox, SIGNAL(toggled(bool)),
            _ui->authWidgets, SLOT(setEnabled(bool)));

    loadProxySettings();
    loadBWLimitSettings();

    // proxy
    connect(_ui->typeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(saveProxySettings()));
    connect(_ui->proxyButtonGroup, SIGNAL(buttonClicked(int)), SLOT(saveProxySettings()));
    connect(_ui->hostLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->userLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->passwordLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->portSpinBox, SIGNAL(editingFinished()), SLOT(saveProxySettings()));

    connect(_ui->uploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->noUploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->autoUploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->downloadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->noDownloadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->downloadSpinBox, SIGNAL(valueChanged(int)), SLOT(saveBWLimitSettings()));
    connect(_ui->uploadSpinBox, SIGNAL(valueChanged(int)), SLOT(saveBWLimitSettings()));
}

NetworkSettings::~NetworkSettings()
{
    delete _ui;
}

void NetworkSettings::loadProxySettings()
{
    // load current proxy settings
    OCC::ConfigFile cfgFile;
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

void NetworkSettings::loadBWLimitSettings()
{
    ConfigFile cfgFile;
    _ui->downloadLimitRadioButton->setChecked(cfgFile.useDownloadLimit());
    int uploadLimit = cfgFile.useUploadLimit();
    if ( uploadLimit >= 1 ) {
        _ui->uploadLimitRadioButton->setChecked(true);
    } else if (uploadLimit == 0){
        _ui->noUploadLimitRadioButton->setChecked(true);
    } else {
        _ui->autoUploadLimitRadioButton->setChecked(true);
    }
    _ui->downloadSpinBox->setValue(cfgFile.downloadLimit());
    _ui->uploadSpinBox->setValue(cfgFile.uploadLimit());
}

void NetworkSettings::saveProxySettings()
{
    ConfigFile cfgFile;

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

    ClientProxy proxy;
    proxy.setupQtProxyFromConfig(); // Refresh the Qt proxy settings as the
    // quota check can happen all the time.

    // ...and set the folders dirty, they refresh their proxy next time they
    // start the sync.
    FolderMan::instance()->setDirtyProxy(true);
}

void NetworkSettings::saveBWLimitSettings()
{
    ConfigFile cfgFile;
    cfgFile.setUseDownloadLimit(_ui->downloadLimitRadioButton->isChecked());

    if (_ui->uploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(1);
    } else if (_ui->noUploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(0);
    } else if (_ui->autoUploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(-1);
    }

    cfgFile.setDownloadLimit(_ui->downloadSpinBox->value());
    cfgFile.setUploadLimit(_ui->uploadSpinBox->value());

    FolderMan::instance()->setDirtyNetworkLimits();
}

} // namespace OCC
