/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "remotepermissions.h"
#include <cstring>

namespace OCC {

static const char letters[] = " WDNVCKRSMm";


template <typename Char>
void RemotePermissions::fromArray(const Char *p)
{
    _value = notNullMask;
    if (!p)
        return;
    while (*p) {
        if (auto res = std::strchr(letters, static_cast<char>(*p)))
            _value |= (1 << (res - letters));
        ++p;
    }
}

QByteArray RemotePermissions::toDbValue() const
{
    QByteArray result;
    if (isNull())
        return result;
    result.reserve(PermissionsCount);
    for (uint i = 1; i <= PermissionsCount; ++i) {
        if (_value & (1 << i))
            result.append(letters[i]);
    }
    if (result.isEmpty()) {
        // Make sure it is not empty so we can differentiate null and empty permissions
        result.append(' ');
    }
    return result;
}

QByteArray RemotePermissions::toString() const
{
    return toDbValue();
}

RemotePermissions RemotePermissions::fromDbValue(const QByteArray &value)
{
    if (value.isEmpty())
        return RemotePermissions();
    RemotePermissions perm;
    perm.fromArray(value.constData());
    return perm;
}

RemotePermissions RemotePermissions::fromServerString(const QString &value)
{
    RemotePermissions perm;
    perm.fromArray(value.utf16());
    return perm;
}

} // namespace OCC
