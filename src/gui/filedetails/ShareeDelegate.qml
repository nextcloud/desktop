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
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

import "../tray"

ItemDelegate {
    id: root

    text: model.display

    contentItem: RowLayout {
        height: visible ? implicitHeight : 0

        Loader {
              id: shareeIconLoader

              Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

              active: model.icon !== ""

              sourceComponent: Image {
                  id: shareeIcon

                  horizontalAlignment: Qt.AlignLeft
                  verticalAlignment: Qt.AlignVCenter

                  width: height
                  height: shareeLabel.height

                  smooth: true
                  antialiasing: true
                  mipmap: true
                  fillMode: Image.PreserveAspectFit

                  source: model.icon

                  sourceSize: Qt.size(shareeIcon.height * 1.0, shareeIcon.height * 1.0)
              }
        }

        EnforcedPlainTextLabel {
            id: shareeLabel
            Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.iconWidth
            Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

            Layout.fillWidth: true

            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            text: model.display
        }
    }
}
