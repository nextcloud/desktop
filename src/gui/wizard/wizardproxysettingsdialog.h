/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QDialog>
#include <QNetworkProxy>

#include "ui_proxysettings.h"

namespace OCC {

class WizardProxySettingsDialog : public QDialog
{
    Q_OBJECT
public:
    enum class ProxyAuthentication {
        AuthenticationRequired,
        NoAuthentication,
    };

    struct WizardProxySettings
    {
        QString _user;
        QString _password;
        QString _host;
        quint16 _port;
        ProxyAuthentication _needsAuth = ProxyAuthentication::NoAuthentication;
        QNetworkProxy::ProxyType _proxyType = QNetworkProxy::NoProxy;

        bool operator == (const WizardProxySettings &other) const = default;
    };

    explicit WizardProxySettingsDialog(QUrl serverURL,
                                       WizardProxySettings proxySettings,
                                       QWidget *parent = nullptr);

    void setServerUrl(QUrl serverUrl);

    void setProxySettings(WizardProxySettings proxySettings);

Q_SIGNALS:
    void proxySettingsAccepted(const OCC::WizardProxySettingsDialog::WizardProxySettings &proxySettings);

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

    WizardProxySettings _settings;
};

}
