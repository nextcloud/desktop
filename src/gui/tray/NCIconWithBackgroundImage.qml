/*
 * Copyright (C) 2023 by Oleksandr Zolotov <alex@nextcloud.com>
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

import QtQuick
import Style

Image {
    id: root

    property alias icon: icon

    cache: true
    mipmap: true
    fillMode: Image.PreserveAspectFit

    Image {
        id: icon

        anchors.centerIn: parent

        cache: true
        mipmap: true
        fillMode: Image.PreserveAspectFit
        visible: source !== ""
    }
}
