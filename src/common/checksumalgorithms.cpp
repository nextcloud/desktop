/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "checksumalgorithms.h"

using namespace OCC;
CheckSums::Algorithm CheckSums::fromByteArray(const QByteArray &s)
{
    // assert to ensure that all keys are uppercase
    Q_ASSERT([] {
        // ensure to run the check only once
        static bool once = [] {
            for (const auto &a : All) {
                const QString s = CheckSums::toQString(a.first);
                if (s != s.toUpper()) {
                    return false;
                }
            }
            return true;
        }();
        return once;
    }());
    return fromName(s.toUpper().constData());
}

#include "moc_checksumalgorithms.cpp"
