/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
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
import QtQuick.Layouts

import Style
import com.nextcloud.desktopclient 1.0 as NC
import "./tray"

ColumnLayout {
    NC.EmojiModel {
        id: emojiModel
    }

    signal chosen(string emoji)

    spacing: 0

    FontMetrics {
        id: metrics
    }

    ListView {
        id: headerLayout
        Layout.fillWidth: true
        Layout.margins: 1
        implicitWidth: contentItem.childrenRect.width
        implicitHeight: metrics.height * 2

        orientation: ListView.Horizontal

        model: emojiModel.emojiCategoriesModel

        delegate: ItemDelegate {
            id: headerDelegate
            width: metrics.height * 2
            height: headerLayout.height

            background: Rectangle {
                color: palette.highlight
                visible: ListView.isCurrentItem ||
                         headerDelegate.highlighted ||
                         headerDelegate.checked ||
                         headerDelegate.down ||
                         headerDelegate.hovered
                radius: Style.slightlyRoundedButtonRadius
            }

            contentItem: EnforcedPlainTextLabel {
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: emoji
            }

            Rectangle {
                anchors.bottom: parent.bottom

                width: parent.width
                height: Style.thickBorderWidth

                visible: ListView.isCurrentItem

                color: palette.dark
            }


            onClicked: {
                emojiModel.setCategory(label)
            }
        }

    }

    Rectangle {
        height: Style.normalBorderWidth
        Layout.fillWidth: true
        color: palette.dark
    }

    GridView {
        id: emojiView
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: metrics.height * 8
        Layout.margins: Style.normalBorderWidth

        cellWidth: metrics.height * 2
        cellHeight: metrics.height * 2

        boundsBehavior: Flickable.DragOverBounds
        clip: true

        model: emojiModel.model

        delegate: ItemDelegate {
            id: emojiDelegate

            width: metrics.height * 2
            height: metrics.height * 2

            background: Rectangle {
                color: palette.highlight
                visible: ListView.isCurrentItem || emojiDelegate.highlighted || emojiDelegate.checked || emojiDelegate.down || emojiDelegate.hovered
                radius: Style.slightlyRoundedButtonRadius
            }

            contentItem: EnforcedPlainTextLabel {
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: modelData === undefined ? "" : modelData.unicode
            }

            onClicked: {
                chosen(modelData.unicode);
                emojiModel.emojiUsed(modelData);
            }
        }

        EnforcedPlainTextLabel {
            id: placeholderMessage
            width: parent.width * 0.8
            anchors.centerIn: parent
            text: qsTr("No recent emojis")
            wrapMode: Text.Wrap
            font.bold: true
            visible: emojiView.count === 0
        }

        ScrollBar.vertical: ScrollBar {}
        
    }

}
