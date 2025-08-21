/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>
#include <QNetworkProxy>

#include "ui_proxysettings.h"

namespace OCC {

class WizardProxySettings : public QDialog
{
    Q_OBJECT
public:
    enum class ProxyAuthentication {
        AuthenticationRequired,
        NoAuthentication,
    };

    explicit WizardProxySettings(QUrl serverURL, QWidget *parent = nullptr);

Q_SIGNALS:
    void proxySettingsAccepted(QString user,
                               QString password,
                               QString host,
                               int port,
                               WizardProxySettings::ProxyAuthentication needsAuth,
                               QNetworkProxy::ProxyType proxyType);

private Q_SLOTS:
    /// Red marking of host field if empty and enabled
    void checkEmptyProxyHost();

    void checkEmptyProxyCredentials();

    void checkAccountLocalhost();

    void validateProxySettings();

    void settingsDone();

private:
    Ui_ProxySettings _ui{};

    QUrl _serverURL;

    bool _valid = false;

    QString _user;
    QString _password;
    QString _host;
    int _port;
    ProxyAuthentication _needsAuth = ProxyAuthentication::NoAuthentication;
    QNetworkProxy::ProxyType _proxyType = QNetworkProxy::NoProxy;
};

}
