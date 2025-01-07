/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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
import QtQuick.Controls
import Style

BusyIndicator {
    id: root

    property color color: palette.dark
    property string imageSource: "image://svgimage-custom-color/change.svg/"

    property int imageSourceSizeWidth: 64
    property int imageSourceSizeHeight: 64

    contentItem: Image {
        id: contentImage

        property bool colourableImage: root.color && root.imageSource.startsWith("image://svgimage-custom-color/")

        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter

        source: colourableImage ? root.imageSource + root.color : root.imageSource
        sourceSize.width: root.imageSourceSizeWidth
        sourceSize.height: root.imageSourceSizeHeight
        fillMode: Image.PreserveAspectFit

        mipmap: true

        RotationAnimator {
            target: contentImage
            running: root.running
            onRunningChanged: contentImage.rotation = 0
            from: 0
            to: 360
            loops: Animation.Infinite
            duration: 3000
        }
    }
}
