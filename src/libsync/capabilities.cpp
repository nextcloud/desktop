/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "capabilities.h"

#include "configfile.h"

#include <QVariantMap>

namespace OCC {


Capabilities::Capabilities(const QVariantMap &capabilities)
    : _capabilities(capabilities)
{
}

bool Capabilities::shareAPI() const
{
    if (_capabilities["files_sharing"].toMap().contains("api_enabled")) {
        return _capabilities["files_sharing"].toMap()["api_enabled"].toBool();
    } else {
        // This was later added so if it is not present just assume the API is enabled.
        return true;
    }
}

bool Capabilities::sharePublicLink() const
{
    return shareAPI() && _capabilities["files_sharing"].toMap()["public"].toMap()["enabled"].toBool();
}

bool Capabilities::sharePublicLinkAllowUpload() const
{
    return  _capabilities["files_sharing"].toMap()["public"].toMap()["upload"].toBool();
}

bool Capabilities::sharePublicLinkEnforcePassword() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["password"].toMap()["enforced"].toBool();
}

bool Capabilities::sharePublicLinkEnforceExpireDate() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["enforced"].toBool();
}

int Capabilities::sharePublicLinkExpireDateDays() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["days"].toInt();
}

bool Capabilities::shareResharing() const
{
    return _capabilities["files_sharing"].toMap()["resharing"].toBool();
}

QList<QByteArray> Capabilities::supportedChecksumTypesAdvertised() const
{
    return QList<QByteArray>();
}

QList<QByteArray> Capabilities::supportedChecksumTypes() const
{
    auto list = supportedChecksumTypesAdvertised();
    QByteArray cfgType = ConfigFile().transmissionChecksum().toLatin1();
    if (!cfgType.isEmpty()) {
        list.prepend(cfgType);
    }
    return list;
}

QByteArray Capabilities::preferredChecksumType() const
{
    auto list = supportedChecksumTypes();
    if (list.isEmpty()) {
        return QByteArray();
    }
    return list.first();
}

}
