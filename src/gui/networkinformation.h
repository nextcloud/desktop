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

class OWNCLOUDGUI_EXPORT NetworkInformation : public QObject
{
    Q_OBJECT

public:
    static void initialize();
    static NetworkInformation *instance();

    bool isMetered();

    using Feature = QNetworkInformation::Feature;
    using Features = QNetworkInformation::Features;
    using Reachability = QNetworkInformation::Reachability;

    bool supports(Features features) const;

Q_SIGNALS:
    void isMeteredChanged(bool isMetered);
    void reachabilityChanged(NetworkInformation::Reachability reachability);

private:
    static NetworkInformation *_instance;
};

}
