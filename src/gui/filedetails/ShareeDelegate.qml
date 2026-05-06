/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls

import com.strato.hidrivenext.desktopclient
import Style

import "../tray"

ItemDelegate {
    id: root

    text: model.display

    background: Rectangle {
        color: root.hovered ? root.pressed ? Style.sesButtonPressed : Style.sesHover : Style.sesWhite
    }

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

            color: Style.sesTrayFontColor
            font.pixelSize: Style.sesFontPixelSize
            font.weight: Style.sesFontNormalWeight

            Layout.fillWidth: true

            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            text: model.display
        }
    }
}
