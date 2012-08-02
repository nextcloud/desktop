/*
 * Copyright (C) by Thomas Mueller <thomas.mueller@tmit.eu>
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

#include <QNetworkProxy>

#include "proxydialog.h"

#include "mirallconfigfile.h"


Mirall::ProxyDialog::ProxyDialog( QWidget* parent )
    : QDialog(parent)
{
    setupUi(this);

    // designer is buggy, so do it programmatically
    manualSettings->setEnabled(false);

#if QT_VERSION >= 0x040700
    hostLineEdit->setPlaceholderText(tr("Hostname of proxy server"));
    userLineEdit->setPlaceholderText(tr("Username for proxy server"));
    passwordLineEdit->setPlaceholderText(tr("Password for proxy server"));
#endif

    // load current proxy settings
    Mirall::MirallConfigFile cfgFile;
    if (cfgFile.proxyType() == QNetworkProxy::NoProxy)
        noProxyRadioButton->setChecked(true);
    if (cfgFile.proxyType() == QNetworkProxy::DefaultProxy)
        systemProxyRadioButton->setChecked(true);
    if (cfgFile.proxyType() == QNetworkProxy::Socks5Proxy)
    {
        manualProxyRadioButton->setChecked(true);
        hostLineEdit->setText(cfgFile.proxyHostName());
        portSpinBox->setValue(cfgFile.proxyPort());
        if (!cfgFile.proxyUser().isEmpty())
        {
            authRequiredcheckBox->setChecked(true);
            userLineEdit->setText(cfgFile.proxyUser());
            passwordLineEdit->setText(cfgFile.proxyPassword());
        }
    }
}

void Mirall::ProxyDialog::saveSettings()
{
    Mirall::MirallConfigFile cfgFile;

    if (noProxyRadioButton->isChecked())
    {
        cfgFile.setProxyType(QNetworkProxy::NoProxy);
    }
    if (systemProxyRadioButton->isChecked())
    {
        cfgFile.setProxyType(QNetworkProxy::DefaultProxy);
    }
    if (manualProxyRadioButton->isChecked())
    {
        if (authRequiredcheckBox->isChecked())
        {
            QString user = userLineEdit->text();
            QString pass = passwordLineEdit->text();
            cfgFile.setProxyType(QNetworkProxy::Socks5Proxy, hostLineEdit->text(), portSpinBox->value(), user, pass);
        }
        else
        {
            cfgFile.setProxyType(QNetworkProxy::Socks5Proxy, hostLineEdit->text(), portSpinBox->value(), QString(), QString());
        }
    }

}


void Mirall::ProxyDialog::on_authRequiredcheckBox_stateChanged(int state)
{
    bool e = (state == Qt::Checked);
    userLineEdit->setEnabled(e);
    passwordLineEdit->setEnabled(e);
    proxyUserLabel->setEnabled(e);
    proxyPasswordLabel->setEnabled(e);
}

void Mirall::ProxyDialog::on_manualProxyRadioButton_toggled(bool checked)
{
    manualSettings->setEnabled(checked);
}

void Mirall::ProxyDialog::on_buttonBox_accepted()
{
    saveSettings();
}
