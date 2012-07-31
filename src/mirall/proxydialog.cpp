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

#if QT_VERSION >= 0x040700
    hostLineEdit->setPlaceholderText(QApplication::translate("proxyDialog", "Hostname of proxy server"));
    userLineEdit->setPlaceholderText(QApplication::translate("proxyDialog", "Username to authenticate on proxy server"));
#endif

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
            cfgFile.setProxyType(QNetworkProxy::Socks5Proxy, hostLabel->text(), portSpinBox->value(), user, pass);
        }
        else
        {
            cfgFile.setProxyType(QNetworkProxy::Socks5Proxy, hostLabel->text(), portSpinBox->value(), QString(), QString());
        }
    }

}


void Mirall::ProxyDialog::on_authRequiredcheckBox_stateChanged(int state)
{
    bool e = (state == Qt::Checked);
    userLineEdit->setEnabled(e);
    passwordLineEdit->setEnabled(e);
}

void Mirall::ProxyDialog::on_manualProxyRadioButton_toggled(bool checked)
{
    hostLineEdit->setEnabled(checked);
    portSpinBox->setEnabled(checked);
    authRequiredcheckBox->setEnabled(checked);
}

void Mirall::ProxyDialog::on_buttonBox_accepted()
{
    saveSettings();
}
