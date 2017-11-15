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

static const char letters[] = " WDNVCKRSMmz";


template <typename Char>
void RemotePermissions::fromArray(const Char *p)
{
    _value = p ? notNullMask : 0;
    if (!p)
        return;
    while (*p) {
        if (auto res = std::strchr(letters, static_cast<char>(*p)))
            _value |= (1 << (res - letters));
        ++p;
    }
}

RemotePermissions::RemotePermissions(const char *p)
{
    fromArray(p);
}

RemotePermissions::RemotePermissions(const QString &s)
{
    fromArray(s.isEmpty() ? nullptr : s.utf16());
}

QByteArray RemotePermissions::toString() const
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

} // namespace OCC
