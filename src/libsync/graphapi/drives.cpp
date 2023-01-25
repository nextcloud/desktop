/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "drives.h"

#include "account.h"

#include <QJsonArray>

#include <OAICollection_of_drives.h>
#include <OAIDrive.h>


using namespace OCC;
using namespace GraphApi;

namespace {

const auto mountpointC = QLatin1String("mountpoint");
const auto personalC = QLatin1String("personal");

// https://github.com/cs3org/reva/blob/0cde0a3735beaa14ebdfd8988c3eb77b3c2ab0e6/pkg/utils/utils.go#L56-L59
const auto sharesIdC = QLatin1String("a0ca6a90-a365-4782-871e-d44447bbc668$a0ca6a90-a365-4782-871e-d44447bbc668");
}

Drives::Drives(const AccountPtr &account, QObject *parent)
    : JsonJob(account, account->url(), QStringLiteral("/graph/v1.0/me/drives"), "GET", {}, {}, parent)
{
}

Drives::~Drives()
{
}

const QList<OpenAPI::OAIDrive> &Drives::drives() const
{
    if (_drives.isEmpty() && parseError().error == QJsonParseError::NoError) {
        OpenAPI::OAICollection_of_drives drives;
        drives.fromJsonObject(data());
        _drives = drives.getValue();
        // At the moment we don't support mountpoints but use the Share Jail
        _drives.erase(std::remove_if(_drives.begin(), _drives.end(), [](const OpenAPI::OAIDrive &it) {
            return it.getDriveType() == mountpointC;
        }),
            _drives.end());
    }
    return _drives;
}

QString Drives::getDriveDisplayName(const OpenAPI::OAIDrive &drive)
{
    if (drive.getDriveType() == personalC) {
        return tr("Personal");
    } else if (drive.getId() == sharesIdC) {
        // don't call it ShareJail
        return tr("Shares");
    }
    return drive.getName();
}

uint32_t Drives::getDrivePriority(const OpenAPI::OAIDrive &drive)
{
    if (drive.getDriveType() == personalC) {
        return 100;
    } else if (drive.getDriveType() == sharesIdC) {
        return 50;
    }
    return 0;
}

namespace OCC::GraphApi {
bool isDriveDisabled(const OpenAPI::OAIDrive &drive)
{
    // this is how disabled spaces are represented in the graph API
    return drive.getRoot().getDeleted().getState() == QLatin1String("trashed");
}
}
