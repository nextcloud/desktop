/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as BasicControls
import QtQuick.Layouts

import Style
import com.nextcloud.desktopclient 1.0 as NC
import "./tray"

ColumnLayout {
    id: root

    property bool showSearch: false
    property int visibleRows: 8
    property string searchText: ""
    readonly property var filteredModel: {
        if (searchText === "") {
            return emojiModel.model
        }

        const needle = searchText.toLowerCase()
        const emojiLists = [
            emojiModel.people,
            emojiModel.nature,
            emojiModel.food,
            emojiModel.activity,
            emojiModel.travel,
            emojiModel.objects,
            emojiModel.symbols,
            emojiModel.flags
        ]
        var results = []
        for (var listIndex = 0; listIndex < emojiLists.length; ++listIndex) {
            const emojis = emojiLists[listIndex]
            for (var emojiIndex = 0; emojiIndex < emojis.length; ++emojiIndex) {
                const emoji = emojis[emojiIndex]
                const shortname = emoji.shortname === undefined ? "" : emoji.shortname.toLowerCase()
                const unicode = emoji.unicode === undefined ? "" : emoji.unicode
                if (shortname.indexOf(needle) !== -1 || unicode.indexOf(searchText) !== -1) {
                    results.push(emoji)
                }
            }
        }
        return results
    }

    NC.EmojiModel {
        id: emojiModel
    }

    signal chosen(string emoji)

    spacing: 0

    FontMetrics {
        id: metrics
    }

    BasicControls.TextField {
        id: searchField

        visible: root.showSearch
        Layout.fillWidth: true
        Layout.preferredHeight: visible ? 32 : 0
        Layout.margins: visible ? Style.smallSpacing : 0
        placeholderText: qsTr("Search emoji")
        selectByMouse: true
        text: root.searchText
        topPadding: 0
        bottomPadding: 0
        leftPadding: Style.standardSpacing
        rightPadding: Style.standardSpacing
        font.pixelSize: Style.pixelSize + 2
        verticalAlignment: TextInput.AlignVCenter
        onTextChanged: root.searchText = text

        background: Rectangle {
            radius: Style.mediumRoundedButtonRadius
            color: palette.base
            border.width: Style.normalBorderWidth
            border.color: searchField.activeFocus ? Style.ncBlue : palette.dark
        }
    }

    ListView {
        id: headerLayout
        Layout.fillWidth: true
        Layout.margins: 1
        implicitWidth: contentItem.childrenRect.width
        implicitHeight: metrics.height * 2

        visible: root.searchText === ""
        Layout.preferredHeight: visible ? implicitHeight : 0
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
                font.pointSize: Style.defaultFontPtSize + 2
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
        Layout.preferredHeight: root.searchText === "" ? Style.normalBorderWidth : 0
        Layout.fillWidth: true
        color: palette.dark
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.preferredHeight: metrics.height * root.visibleRows
        Layout.margins: Style.normalBorderWidth
        clip: true

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        GridView {
            id: emojiView
            width: parent.width
            height: parent.height
            cellWidth: metrics.height * 2
            cellHeight: metrics.height * 2
            boundsBehavior: Flickable.DragOverBounds
            model: root.filteredModel

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
                    font.pointSize: Style.defaultFontPtSize + 4
                }

                ToolTip {
                    popupType: Qt.platform.os === "windows" ? Popup.Item : Popup.Native
                    text: modelData === undefined ? "" : modelData.shortname
                    visible: emojiDelegate.hovered
                    delay: Qt.styleHints.mousePressAndHoldInterval
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
                text: root.searchText === "" ? qsTr("No recent emojis") : qsTr("No emojis found")
                wrapMode: Text.Wrap
                font.bold: true
                visible: emojiView.count === 0
            }
        }
    }
}
