/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "networkinformation.h"

#include <QLoggingCategory>

using namespace OCC;

Q_LOGGING_CATEGORY(lcNetInfo, "gui.netinfo", QtInfoMsg)

NetworkInformation *NetworkInformation::_instance = nullptr;

namespace {

void loadQNetworkInformationBackend()
{
    if (!QNetworkInformation::loadDefaultBackend()) {
        qCWarning(lcNetInfo) << "Failed to load default backend of QNetworkInformation.";
        if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Metered)) {
            qCWarning(lcNetInfo) << "Failed to load backend of QNetworkInformation by metered feature.";
            if (!QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability)) {
                qCWarning(lcNetInfo) << "Failed to load backend of QNetworkInformation by reachability feature.";
                qCWarning(lcNetInfo) << "Available backends:" << QNetworkInformation::availableBackends().join(QStringLiteral(", "));
                return;
            }
        }
    }
    qCDebug(lcNetInfo) << "Loaded network information backend:" << QNetworkInformation::instance()->backendName();
    qCDebug(lcNetInfo) << "Supported features:" << QNetworkInformation::instance()->supportedFeatures();
    qCDebug(lcNetInfo) << "Available backends:" << QNetworkInformation::availableBackends().join(QStringLiteral(", "));

    if (auto qni = QNetworkInformation::instance()) {
        QObject::connect(qni, &QNetworkInformation::reachabilityChanged, [qni](QNetworkInformation::Reachability reachability) {
            qCInfo(lcNetInfo) << "Connection Status changed to:" << reachability << "captive portal status:" << qni->isBehindCaptivePortal();
        });
    }
}

} // anonymous namespace

NetworkInformation::NetworkInformation() { }

NetworkInformation *NetworkInformation::instance()
{
    if (!_instance) {
        _instance = new NetworkInformation;

        loadQNetworkInformationBackend();

        if (auto qni = QNetworkInformation::instance()) {
            connect(qni, &QNetworkInformation::isMeteredChanged, _instance, &NetworkInformation::isMeteredChanged);
            connect(qni, &QNetworkInformation::reachabilityChanged, _instance, &NetworkInformation::reachabilityChanged);
            connect(qni, &QNetworkInformation::isBehindCaptivePortalChanged, _instance, &NetworkInformation::slotIsBehindCaptivePortalChanged);
        }
    }

    return _instance;
}

bool NetworkInformation::isMetered()
{
    if (auto *qNetInfo = QNetworkInformation::instance()) {
        return qNetInfo->isMetered();
    }

    return false;
}

bool NetworkInformation::supports(Features features) const
{
    if (auto *qNetInfo = QNetworkInformation::instance()) {
        return qNetInfo->supports(features);
    }

    return false;
}

bool NetworkInformation::isForcedCaptivePortal() const
{
    return _forcedCaptivePortal;
}

void NetworkInformation::setForcedCaptivePortal(bool onoff)
{
    if (_forcedCaptivePortal != onoff) {
        _forcedCaptivePortal = onoff;
        qCDebug(lcNetInfo) << "Switching forced captive portal to" << onoff;

        bool behindCaptivePortal = false;

        if (auto *qNetInfo = QNetworkInformation::instance()) {
            behindCaptivePortal = qNetInfo->isBehindCaptivePortal();
        }

        if (behindCaptivePortal != _forcedCaptivePortal) {
            Q_EMIT isBehindCaptivePortalChanged(_forcedCaptivePortal);
        }
    }
}

bool NetworkInformation::isBehindCaptivePortal() const
{
    if (_forcedCaptivePortal) {
        return true;
    }

    if (auto *qNetInfo = QNetworkInformation::instance()) {
        return qNetInfo->isBehindCaptivePortal();
    }

    return false;
}

void NetworkInformation::slotIsBehindCaptivePortalChanged(bool state)
{
    qCDebug(lcNetInfo) << "OS signals behind captive portal changed to" << state << "forced captive portal flag:" << _forcedCaptivePortal;

    if (!_forcedCaptivePortal) {
        Q_EMIT isBehindCaptivePortalChanged(state);
    }
}
