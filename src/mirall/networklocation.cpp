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

#include "mirall/networklocation.h"

#include <QProcess>

namespace Mirall
{

NetworkLocation::NetworkLocation(const QString &encoded)
    : _encoded(encoded)
{
}

NetworkLocation::NetworkLocation()
{
}

NetworkLocation::~NetworkLocation()
{
}

/**
 * for now our data is just the MAC address of the default gateway
 */
NetworkLocation NetworkLocation::currentLocation()
{
    QProcess ip;
    ip.start(QLatin1String("/sbin/ip"), QStringList() << QLatin1String("route"));

    if (!ip.waitForStarted())
        return NetworkLocation();

    if (!ip.waitForFinished())
        return NetworkLocation();

    QByteArray gwIp;
    while (ip.canReadLine()) {
        QByteArray line = ip.readLine();
        if ( line.startsWith("default") ) { // krazy:exclude=strings
            QList<QByteArray> parts = line.split(' ');
            gwIp = parts[2];
            break;
        }
    }
    if (gwIp.isEmpty())
            return NetworkLocation();

    QProcess arp;
    arp.start(QLatin1String("/sbin/arp"), QStringList() << QLatin1String("-a"));

    if (!arp.waitForStarted())
        return NetworkLocation();

    if (!arp.waitForFinished())
        return NetworkLocation();

    QByteArray gwMAC;
    while (arp.canReadLine()) {
        QByteArray line = arp.readLine();
        if (line.contains(gwIp)) {
            QList<QByteArray> parts = line.split(' ');
            gwMAC = parts[3];
            break;
        }
    }
    if (gwMAC.isEmpty())
        return NetworkLocation();

    return NetworkLocation(QString::fromLatin1(gwMAC));
}


NetworkLocation::Proximity NetworkLocation::compareWith(const NetworkLocation &location) const
{
    if (location.encoded().isEmpty() || encoded().isEmpty())
        return Unknown;
    if (location.encoded() == encoded())
        return Same;
    return Different;
}

QString NetworkLocation::encoded() const
{
    return _encoded;
}

}

