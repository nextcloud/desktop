/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include "csync.h"
#include "moc_csync.cpp"

#include <QMetaEnum>

QDebug operator<<(QDebug debug, const SyncInstructions &enumValue)
{
    static const QMetaObject *mo = qt_getEnumMetaObject(SyncInstruction());
    static const int enumIdx = mo->indexOfEnumerator(qt_getEnumName(SyncInstruction()));
    static const QMetaEnum me = mo->enumerator(enumIdx);
    QDebugStateSaver saver(debug);
    debug.nospace().noquote() << me.enumName() << "(" << me.valueToKeys(enumValue) << ")";
    return debug;
}
