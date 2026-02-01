/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import Qt5Compat.GraphicalEffects
import com.nextcloud.desktopclient

RowLayout {
    id: root

    required property color adaptiveTextColor

    property variant activityData: {{}}

    property variant activity: {{}}

    property bool showDismissButton: false

    property bool childHovered: fileDetailsButton.hovered || dismissActionButton.hovered

    property int iconSize: Style.trayListItemIconSize

    signal dismissButtonClicked()

    spacing: Style.standardSpacing

    Item {
        id: thumbnailItem

        readonly property int imageWidth: width * (1 - Style.thumbnailImageSizeReduction)
        readonly property int imageHeight: height * (1 - Style.thumbnailImageSizeReduction)
        readonly property int thumbnailRadius: model.thumbnail && model.thumbnail.isUserAvatar ? width / 2 : 3

        implicitWidth: root.iconSize
        implicitHeight: model.thumbnail && model.thumbnail.isMimeTypeIcon ? root.iconSize * 0.9 : root.iconSize

        Loader {
            id: thumbnailImageLoader
            anchors.fill: parent
            active: model.thumbnail !== undefined

            sourceComponent: Item {
                anchors.fill: parent
                readonly property int paintedWidth: model.thumbnail.isMimeTypeIcon ? thumbnailImage.paintedWidth * 0.8 : thumbnailImage.paintedWidth
                readonly property int paintedHeight: model.thumbnail.isMimeTypeIcon ? thumbnailImage.paintedHeight * 0.55 : thumbnailImage.paintedHeight

                Image {
                    id: thumbnailImage
                    width: thumbnailItem.imageWidth
                    height: thumbnailItem.imageHeight
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    cache: true
                    fillMode: Image.PreserveAspectFit
                    source: model.thumbnail.source
                    visible: false
                    sourceSize.height: 64
                    sourceSize.width: 64
                }

                Rectangle {
                    id: mask
                    color: "white"
                    radius: thumbnailItem.thumbnailRadius
                    anchors.fill: thumbnailImage
                    visible: false
                    width: thumbnailImage.paintedWidth
                    height: thumbnailImage.paintedHeight
                }

                OpacityMask {
                    anchors.fill: thumbnailImage
                    source: thumbnailImage
                    maskSource: mask
                    visible: model.thumbnail !== undefined
                }
            }
        }

        Image {
            id: activityIcon
            width: model.thumbnail !== undefined ? parent.width * 0.4 : thumbnailItem.imageWidth
            height: model.thumbnail !== undefined ? width : width * 0.9

            // Prevent bad access into unloaded item properties
            readonly property int thumbnailPaintedWidth: thumbnailImageLoader.item ? thumbnailImageLoader.item.paintedWidth : 0
            readonly property int thumbnailPaintedHeight: thumbnailImageLoader.item ? thumbnailImageLoader.item.paintedHeight : 0

            readonly property int negativeLeftMargin: -((width / 2) +
                                                        ((width - paintedWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailItem.imageWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailPaintedWidth) / 2) +
                                                        (thumbnailItem.thumbnailRadius / 4))
            readonly property int negativeTopMargin: -((height / 2) +
                                                       ((height - paintedHeight) / 2) +
                                                       ((thumbnailImageLoader.height - thumbnailItem.imageHeight) / 4) +
                                                       ((thumbnailImageLoader.height - thumbnailPaintedHeight) / 4) +
                                                       (thumbnailItem.thumbnailRadius / 4))

            anchors.verticalCenter: if(model.thumbnail === undefined) parent.verticalCenter
            anchors.left: model.thumbnail === undefined ? parent.left : thumbnailImageLoader.right
            anchors.leftMargin: if(model.thumbnail !== undefined) negativeLeftMargin
            anchors.top: if(model.thumbnail !== undefined) thumbnailImageLoader.bottom
            anchors.topMargin: if(model.thumbnail !== undefined) negativeTopMargin

            cache: true
            fillMode: Image.PreserveAspectFit
            source: model.icon + "/" + palette.text
            sourceSize.height: 64
            sourceSize.width: 64
            mipmap: true // Addresses grainy downscale
        }
    }

    ColumnLayout {
        id: activityContentLayout

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.maximumWidth: root.width - Style.standardSpacing - root.iconSize
        implicitWidth: root.width - Style.standardSpacing - root.iconSize

        spacing: Style.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                id: activityTextTitle
                text: (root.activityData.type === "Activity" || root.activityData.type === "Notification") ? root.activityData.subject : root.activityData.message

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.topLinePixelSize
                visible: text !== ""
                color: root.adaptiveTextColor
            }

            Item {
                Layout.fillWidth: true
                Layout.leftMargin: -Style.trayHorizontalMargin
            }

            EnforcedPlainTextLabel {
                id: activityTextDateTime

                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                height: (text === "") ? 0 : implicitHeight

                text: root.activityData.dateTime
                font.pixelSize: Style.subLinePixelSize
                visible: text !== ""
                color: root.adaptiveTextColor
            }

            Row {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                spacing: Style.extraSmallSpacing

                Button {
                    id: fileDetailsButton

                    width: Style.activityListButtonWidth
                    height: Style.activityListButtonHeight

                    icon.source: "image://svgimage-custom-color/more.svg/" + palette.buttonText
                    icon.width: Style.activityListButtonIconSize
                    icon.height: Style.activityListButtonIconSize

                    ToolTip {
                        popupType: Qt.platform.os === "windows" ? Popup.Item : Qt.platform.os === "windows" ? Popup.Item : Popup.Native
                        text: qsTr("Open file details")
                        visible: parent.hovered
                    }

                    display: Button.IconOnly
                    visible: model.showFileDetails
                    onClicked: fileMoreButtonMenu.visible ? fileMoreButtonMenu.close() : fileMoreButtonMenu.popup()

                    AutoSizingMenu {
                        id: fileMoreButtonMenu
                        closePolicy: Menu.CloseOnPressOutsideParent | Menu.CloseOnEscape

                        MenuItem {
                            height: visible ? implicitHeight : 0
                            text: qsTr("File details")
                            font.pixelSize: Style.topLinePixelSize
                            hoverEnabled: true
                            onClicked: Systray.presentShareViewInTray(model.openablePath)
                        }

                        MenuItem {
                            visible: model.serverHasIntegration
                            height: visible ? implicitHeight : 0
                            text: qsTr("File actions")
                            font.pixelSize: Style.topLinePixelSize
                            hoverEnabled: true
                            onClicked: Systray.presentFileActionsViewInSystray(model.openablePath)
                        }
                    }
                }

                Button {
                    id: dismissActionButton

                    width: Style.activityListButtonWidth
                    height: Style.activityListButtonHeight

                    icon.source: "image://svgimage-custom-color/clear.svg/" + palette.buttonText
                    icon.width: Style.activityListButtonIconSize
                    icon.height: Style.activityListButtonIconSize

                    display: Button.IconOnly

                    ToolTip {
                        popupType: Qt.platform.os === "windows" ? Popup.Item : Qt.platform.os === "windows" ? Popup.Item : Popup.Native
                        text: qsTr("Dismiss")
                        visible: parent.hovered
                    }

                    visible: root.showDismissButton
                    onClicked: root.dismissButtonClicked()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: activityTextInfo.visible || talkReplyMessageSent.visible || activityActions.visible

            EnforcedPlainTextLabel {
                id: activityTextInfo

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                text: (root.activityData.type === "Sync") ? root.activityData.subject
                                                          : (root.activityData.type === "File") ? root.activityData.subject
                                                                                                : (root.activityData.type === "Notification") ? root.activityData.message
                                                                                                                                              : ""
                height: (text === "") ? 0 : implicitHeight
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.subLinePixelSize
                visible: text !== ""
                color: root.adaptiveTextColor
            }

            Item {
                Layout.fillWidth: true
                visible: !talkReplyMessageSent.visible
            }

            EnforcedPlainTextLabel {
                id: talkReplyMessageSent

                height: (text === "") ? 0 : implicitHeight
                Layout.maximumWidth: parent.width / 2
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                text: root.activityData.messageSent
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.topLinePixelSize
                visible: text !== ""
                color: root.adaptiveTextColor
            }

            ActivityItemActions {
                id: activityActions

                visible: !isFileActivityList && activityData.linksForActionButtons.length > 0

                Layout.fillWidth: true
                Layout.leftMargin: Style.trayListItemIconSize + Style.trayHorizontalMargin
                Layout.preferredHeight: Style.standardPrimaryButtonHeight
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                displayActions: activityData.displayActions
                objectType: activityData.objectType
                linksForActionButtons: activityData.linksForActionButtons
                linksContextMenu: activityData.linksContextMenu

                maxActionButtons: activityModel.maxActionButtons

                onTriggerAction: activityModel.slotTriggerAction(activityData.activityIndex, actionIndex)

                onShowReplyField: isTalkReplyOptionVisible = true
                talkReplyButtonVisible: root.activityData.messageSent === "" && !isTalkReplyOptionVisible
            }
        }
    }
}
