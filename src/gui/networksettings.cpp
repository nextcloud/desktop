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

#include "theme.h"
#include "configfile.h"
#include "application.h"
#include "utility.h"
#include "configfile.h"
#include "folderman.h"

#include <QNetworkProxy>

namespace OCC {

NetworkSettings::NetworkSettings(QWidget *parent)
    : QWidget(parent)
    , _ui(new Ui::NetworkSettings)
{
    _ui->setupUi(this);

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
    connect(_ui->authRequiredcheckBox, SIGNAL(toggled(bool)),
        _ui->authWidgets, SLOT(setEnabled(bool)));

    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
        _ui->manualSettings, SLOT(setEnabled(bool)));
    connect(_ui->manualProxyRadioButton, SIGNAL(toggled(bool)),
        _ui->typeComboBox, SLOT(setEnabled(bool)));

    loadProxySettings();
    loadBWLimitSettings();

    // proxy
    connect(_ui->typeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(saveProxySettings()));
    connect(_ui->proxyButtonGroup, SIGNAL(buttonClicked(int)), SLOT(saveProxySettings()));
    connect(_ui->hostLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->userLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->passwordLineEdit, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->portSpinBox, SIGNAL(editingFinished()), SLOT(saveProxySettings()));
    connect(_ui->authRequiredcheckBox, SIGNAL(toggled(bool)), SLOT(saveProxySettings()));

    connect(_ui->uploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->noUploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->autoUploadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->downloadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->noDownloadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->autoDownloadLimitRadioButton, SIGNAL(clicked()), SLOT(saveBWLimitSettings()));
    connect(_ui->downloadSpinBox, SIGNAL(valueChanged(int)), SLOT(saveBWLimitSettings()));
    connect(_ui->uploadSpinBox, SIGNAL(valueChanged(int)), SLOT(saveBWLimitSettings()));
}

NetworkSettings::~NetworkSettings()
{
    delete _ui;
}

QSize NetworkSettings::sizeHint() const
{
    return QSize(ownCloudGui::settingsDialogSize().width(), QWidget::sizeHint().height());
}

void NetworkSettings::loadProxySettings()
{
    if (Theme::instance()->forceSystemNetworkProxy()) {
        _ui->systemProxyRadioButton->setChecked(true);
        _ui->proxyGroupBox->setEnabled(false);
        return;
    }
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
    _ui->authRequiredcheckBox->setChecked(cfgFile.proxyNeedsAuth());
    _ui->userLineEdit->setText(cfgFile.proxyUser());
    _ui->passwordLineEdit->setText(cfgFile.proxyPassword());
}

void NetworkSettings::loadBWLimitSettings()
{
    ConfigFile cfgFile;

#if QT_VERSION < QT_VERSION_CHECK(5, 3, 3)
    // QNAM bandwidth limiting only works with versions of Qt greater or equal to 5.3.3
    // (It needs Qt commits 097b641 and b99fa32)

    const char *v = qVersion(); // "x.y.z";
    if (QLatin1String(v) < QLatin1String("5.3.3")) {
        QString tooltip = tr("Qt >= 5.4 is required in order to use the bandwidth limit");
        _ui->downloadBox->setEnabled(false);
        _ui->uploadBox->setEnabled(false);
        _ui->downloadBox->setToolTip(tooltip);
        _ui->uploadBox->setToolTip(tooltip);
        _ui->noDownloadLimitRadioButton->setChecked(true);
        _ui->noUploadLimitRadioButton->setChecked(true);
        if (cfgFile.useUploadLimit() != 0 || cfgFile.useDownloadLimit() != 0) {
            // Update from old mirall that was using neon propagator jobs.
            saveBWLimitSettings();
        }
        return;
    }

#endif
    int useDownloadLimit = cfgFile.useDownloadLimit();
    if (useDownloadLimit >= 1) {
        _ui->downloadLimitRadioButton->setChecked(true);
    } else if (useDownloadLimit == 0) {
        _ui->noDownloadLimitRadioButton->setChecked(true);
    } else {
        _ui->autoDownloadLimitRadioButton->setChecked(true);
    }
    _ui->downloadSpinBox->setValue(cfgFile.downloadLimit());

    int useUploadLimit = cfgFile.useUploadLimit();
    if (useUploadLimit >= 1) {
        _ui->uploadLimitRadioButton->setChecked(true);
    } else if (useUploadLimit == 0) {
        _ui->noUploadLimitRadioButton->setChecked(true);
    } else {
        _ui->autoUploadLimitRadioButton->setChecked(true);
    }
    _ui->uploadSpinBox->setValue(cfgFile.uploadLimit());
}

void NetworkSettings::saveProxySettings()
{
    ConfigFile cfgFile;

    if (_ui->noProxyRadioButton->isChecked()) {
        cfgFile.setProxyType(QNetworkProxy::NoProxy);
    } else if (_ui->systemProxyRadioButton->isChecked()) {
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
    if (_ui->downloadLimitRadioButton->isChecked()) {
        cfgFile.setUseDownloadLimit(1);
    } else if (_ui->noDownloadLimitRadioButton->isChecked()) {
        cfgFile.setUseDownloadLimit(0);
    } else if (_ui->autoDownloadLimitRadioButton->isChecked()) {
        cfgFile.setUseDownloadLimit(-1);
    }
    cfgFile.setDownloadLimit(_ui->downloadSpinBox->value());

    if (_ui->uploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(1);
    } else if (_ui->noUploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(0);
    } else if (_ui->autoUploadLimitRadioButton->isChecked()) {
        cfgFile.setUseUploadLimit(-1);
    }
    cfgFile.setUploadLimit(_ui->uploadSpinBox->value());

    FolderMan::instance()->setDirtyNetworkLimits();
}

} // namespace OCC
