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

#pragma once

#include "gui/owncloudguilib.h"

#include <QNetworkInformation>

namespace OCC {

/**
 * @brief Wrapper class for QNetworkInformation
 *
 * This class is used instead of QNetworkInformation so we do not need to check for an instance,
 * and to facilitate debugging by being able to force certain network states (i.e. captive portal).
 */
class OWNCLOUDGUI_EXPORT NetworkInformation : public QObject
{
    Q_OBJECT

public:
    static NetworkInformation *instance();

    bool isMetered();

    using Feature = QNetworkInformation::Feature;
    using Features = QNetworkInformation::Features;
    using Reachability = QNetworkInformation::Reachability;

    bool supports(Features features) const;

    bool isForcedCaptivePortal() const;
    void setForcedCaptivePortal(bool onoff);
    bool isBehindCaptivePortal() const;

Q_SIGNALS:
    void isMeteredChanged(bool isMetered);
    void reachabilityChanged(NetworkInformation::Reachability reachability);
    void isBehindCaptivePortalChanged(bool state);

private Q_SLOTS:
    void slotIsBehindCaptivePortalChanged(bool state);

private:
    NetworkInformation();

    static NetworkInformation *_instance;

    bool _forcedCaptivePortal = false;
};

}
