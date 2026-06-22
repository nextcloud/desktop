/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import Style

BusyIndicator {
    id: root

    property color color: palette.windowText
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
