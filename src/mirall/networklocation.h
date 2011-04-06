/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
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

#ifndef MIRALL_NETWORK_LOCATION_H
#define MIRALL_NETWORK_LOCATION_H

#include <QString>

namespace Mirall {

class NetworkLocation
{
public:

    enum Proximity {
        Unknown,
        Same,
        Different
    };

    /**
     * constructs a location from its encoded
     * form
     */
    NetworkLocation(const QString &encoded);

    /**
     * Unknown location
     */
    NetworkLocation();

    ~NetworkLocation();

    QString encoded() const;

    static NetworkLocation currentLocation();

    Proximity compareWith(const NetworkLocation &location) const;
private:
    QString _encoded;
};

}


#endif
