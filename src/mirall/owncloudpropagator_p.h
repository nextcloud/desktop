/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
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

#pragma once

namespace Mirall {

inline QByteArray parseEtag(const char *header) {
    if (!header)
        return QByteArray();
    QByteArray arr = header;
    arr.replace("-gzip", ""); // https://github.com/owncloud/mirall/issues/1195
    if(arr.length() >= 2 && arr.startsWith('"') && arr.endsWith('"')) {
        arr = arr.mid(1, arr.length() - 2);
    }
    return arr;
}


}
