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
import Qt5Compat.GraphicalEffects

import com.nextcloud.desktopclient
import Style
import "../tray"
import "../"

GridLayout {
    id: root

    signal deleteShare
    signal createNewLinkShare

    signal resetMenu
    signal resetPasswordField
    signal showPasswordSetError(string errorMessage);

    signal toggleHideDownload(bool enable)
    signal toggleAllowEditing(bool enable)
    signal toggleAllowResharing(bool enable)
    signal togglePasswordProtect(bool enable)
    signal toggleExpirationDate(bool enable)
    signal toggleNoteToRecipient(bool enable)
    signal permissionModeChanged(int permissionMode)

    signal setLinkShareLabel(string label)
    signal setExpireDate(var milliseconds) // Since QML ints are only 32 bits, use a variant
    signal setPassword(string password)
    signal setNote(string note)

    property int iconSize: 32
    property FileDetails fileDetails: FileDetails {}
    property StackView rootStackView: StackView {}
    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue

    property bool canCreateLinkShares: true
    property bool serverAllowsResharing: true

    readonly property bool isLinkShare: model.shareType === ShareModel.ShareTypeLink
    readonly property bool isPlaceholderLinkShare: model.shareType === ShareModel.ShareTypePlaceholderLink
    readonly property bool isSecureFileDropPlaceholderLinkShare: model.shareType === ShareModel.ShareTypeSecureFileDropPlaceholderLink
    readonly property bool isInternalLinkShare: model.shareType === ShareModel.ShareTypeInternalLink

    readonly property string text: model.display ?? ""
    readonly property string detailText: model.detailText ?? ""
    readonly property string iconUrl: model.iconUrl ?? ""
    readonly property string avatarUrl: model.avatarUrl ?? ""
    readonly property string link: model.link ?? ""

    anchors.left: parent.left
    anchors.right: parent.right

    columns: 3
    rows: linkDetailLabel.visible ? 1 : 2

    columnSpacing: Style.standardSpacing / 2
    rowSpacing: Style.standardSpacing / 2

    Item {
        id: imageItem

        property bool isAvatar: root.avatarUrl !== ""

        Layout.row: 0
        Layout.column: 0
        Layout.rowSpan: root.rows
        Layout.preferredWidth: root.iconSize
        Layout.preferredHeight: root.iconSize

        Rectangle {
            id: backgroundOrMask
            anchors.fill: parent
            radius: width / 2
            color: root.accentColor
            visible: !imageItem.isAvatar
        }

        Image {
            id: shareIconOrThumbnail

            anchors.centerIn: parent

            verticalAlignment: Image.AlignVCenter
            horizontalAlignment: Image.AlignHCenter
            fillMode: Image.PreserveAspectFit

            source: imageItem.isAvatar ? root.avatarUrl : root.iconUrl + "/white"
            sourceSize.width: imageItem.isAvatar ? root.iconSize : root.iconSize / 2
            sourceSize.height: imageItem.isAvatar ? root.iconSize : root.iconSize / 2

            visible: !imageItem.isAvatar
        }

        OpacityMask {
            anchors.fill: parent
            source: shareIconOrThumbnail
            maskSource: backgroundOrMask
            visible: imageItem.isAvatar
        }
    }

    EnforcedPlainTextLabel {
        id: shareTypeLabel

        Layout.fillWidth: true
        Layout.alignment: linkDetailLabel.visible ? Qt.AlignBottom : Qt.AlignVCenter
        Layout.row: 0
        Layout.column: 1
        Layout.rowSpan: root.rows

        text: root.text
        elide: Text.ElideRight
    }

    EnforcedPlainTextLabel {
        id: linkDetailLabel

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.row: 1
        Layout.column: 1

        text: root.detailText
        elide: Text.ElideRight
        visible: text !== ""
    }

    RowLayout {        
        Layout.row: 0
        Layout.column: 2
        Layout.rowSpan: root.rows
        Layout.fillHeight: true

        spacing: 0

        Button {
            id: createLinkButton

            Layout.alignment: Qt.AlignCenter
            Layout.preferredWidth: Style.activityListButtonWidth
            Layout.preferredHeight: Style.activityListButtonHeight

            icon.source: "image://svgimage-custom-color/add.svg/" + palette.buttonText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize
            display: AbstractButton.IconOnly

            visible: (root.isPlaceholderLinkShare || root.isSecureFileDropPlaceholderLinkShare) && root.canCreateLinkShares
            enabled: visible

            onClicked: root.createNewLinkShare()
        }

        Button {
            id: copyLinkButton

            function copyShareLink() {
                clipboardHelper.text = root.link;
                clipboardHelper.selectAll();
                clipboardHelper.copy();
                clipboardHelper.clear();

                shareLinkCopied = true;
                shareLinkCopyTimer.start();
            }

            property bool shareLinkCopied: false

            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.preferredWidth: shareLinkCopied ? implicitWidth : Style.activityListButtonWidth
            Layout.preferredHeight: Style.activityListButtonHeight

            text: shareLinkCopied ? qsTr("Copied!") : ""

            icon.source: shareLinkCopied ? "image://svgimage-custom-color/copy.svg/" + palette.brightText :
                                           "image://svgimage-custom-color/copy.svg/" + palette.buttonText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize
            display: AbstractButton.IconOnly
            visible: root.isLinkShare || root.isInternalLinkShare
            enabled: visible
            onClicked: copyShareLink()

            Behavior on Layout.preferredWidth {
                SmoothedAnimation { duration: Style.shortAnimationDuration }
            }

            TextEdit {
                id: clipboardHelper
                visible: false
            }

            Timer {
                id: shareLinkCopyTimer
                interval: Style.veryLongAnimationDuration
                onTriggered: copyLinkButton.shareLinkCopied = false
            }
        }

        Button {
            id: moreButton

            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.preferredWidth: Style.activityListButtonWidth
            Layout.preferredHeight: Style.activityListButtonHeight

            icon.source: "image://svgimage-custom-color/more.svg/" + palette.buttonText
            icon.width: Style.activityListButtonIconSize
            icon.height: Style.activityListButtonIconSize
            display: AbstractButton.IconOnly
            visible: !root.isPlaceholderLinkShare && !root.isSecureFileDropPlaceholderLinkShare && !root.isInternalLinkShare
            enabled: visible
            onClicked: root.rootStackView.push(shareDetailsPageComponent, {}, StackView.PushTransition)

            Component {
                id: shareDetailsPageComponent
                ShareDetailsPage {
                    id: shareDetailsPage

                    backgroundsVisible: root.backgroundsVisible
                    accentColor: root.accentColor

                    fileDetails: root.fileDetails
                    shareModelData: model

                    canCreateLinkShares: root.canCreateLinkShares
                    serverAllowsResharing: root.serverAllowsResharing

                    onCloseShareDetails: root.rootStackView.pop(root.rootStackView.initialItem, StackView.PopTransition)

                    onToggleAllowEditing: root.toggleAllowEditing(enable)
                    onToggleAllowResharing: root.toggleAllowResharing(enable)
                    onToggleHideDownload: root.toggleHideDownload(enable)
                    onTogglePasswordProtect: root.togglePasswordProtect(enable)
                    onToggleExpirationDate: root.toggleExpirationDate(enable)
                    onToggleNoteToRecipient: root.toggleNoteToRecipient(enable)
                    onPermissionModeChanged: root.permissionModeChanged(permissionMode)

                    onSetLinkShareLabel: root.setLinkShareLabel(label)
                    onSetExpireDate: root.setExpireDate(milliseconds) // Since QML ints are only 32 bits, use a variant
                    onSetPassword: root.setPassword(password)
                    onSetNote: root.setNote(note)

                    onDeleteShare: {
                        root.deleteShare();
                        closeShareDetails();
                    }
                    onCreateNewLinkShare: {
                        root.createNewLinkShare();
                        closeShareDetails();
                    }

                    Connections {
                        target: root
                        function onResetMenu() { shareDetailsPage.resetMenu() }
                        function onResetPasswordField() { shareDetailsPage.resetPasswordField() }
                        function onShowPasswordSetError(errorMessage) { shareDetailsPage.showPasswordSetError(errorMessage) }
                    }
                }
            }
        }
    }
}
