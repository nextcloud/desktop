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

import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.15

import com.nextcloud.desktopclient 1.0
import Style 1.0

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
