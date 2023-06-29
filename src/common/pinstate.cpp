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

#include "pinstate.h"
#include "moc_pinstate.cpp"

#include <QCoreApplication>

using namespace OCC;

template <>
QString Utility::enumToDisplayName(VfsItemAvailability availability)
{
    switch (availability) {
    case VfsItemAvailability::AlwaysLocal:
        return QCoreApplication::translate("pinstate", "Always available locally");
    case VfsItemAvailability::AllHydrated:
        return QCoreApplication::translate("pinstate", "Currently available locally");
    case VfsItemAvailability::Mixed:
        return QCoreApplication::translate("pinstate", "Some available online only");
    case VfsItemAvailability::AllDehydrated:
        [[fallthrough]];
    case VfsItemAvailability::OnlineOnly:
        return QCoreApplication::translate("pinstate", "Available online only");
    }
    Q_UNREACHABLE();
}
