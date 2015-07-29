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

#include <QVariantMap>

namespace OCC {


Capabilities::Capabilities(const QVariantMap &capabilities)
    : _capabilities(capabilities)
{
}

bool Capabilities::publicLinkEnforcePassword() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["password"].toMap()["enforced"].toBool();
}

bool Capabilities::publicLinkEnforceExpireDate() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["enforced"].toBool();
}

int Capabilities::publicLinkExpireDateDays() const
{
    return _capabilities["files_sharing"].toMap()["public"].toMap()["expire_date"].toMap()["days"].toInt();
}

}
