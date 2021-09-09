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

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import com.nextcloud.desktopclient 1.0 as NC

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
        implicitWidth: contentItem.childrenRect.width
        implicitHeight: metrics.height * 2

        orientation: ListView.Horizontal

        model: emojiModel.emojiCategoriesModel

        delegate: ItemDelegate {
            width: metrics.height * 2
            height: headerLayout.height

            contentItem: Text {
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: emoji
            }

            Rectangle {
                anchors.bottom: parent.bottom

                width: parent.width
                height: 2

                visible: ListView.isCurrentItem

                color: "grey"
            }


            onClicked: {
                emojiModel.setCategory(label)
            }
        }

    }

    Rectangle {
        height: 1
        Layout.fillWidth: true
        color: "grey"
    }

    GridView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: metrics.height * 8

        cellWidth: metrics.height * 2
        cellHeight: metrics.height * 2

        boundsBehavior: Flickable.DragOverBounds
        clip: true

        model: emojiModel.model

        delegate: ItemDelegate {

            width: metrics.height * 2
            height: metrics.height * 2

            contentItem: Text {
                anchors.centerIn: parent
                text: modelData === undefined ? "" : modelData.unicode
            }

            onClicked: {
                chosen(modelData.unicode);
                emojiModel.emojiUsed(modelData);
            }
        }

        ScrollBar.vertical: ScrollBar {}
        
    }

}
