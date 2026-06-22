/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
