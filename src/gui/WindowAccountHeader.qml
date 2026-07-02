/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import Style
import "./tray"

Item {
    id: root

    property string title: ""
    property var user: null

    readonly property string avatarSource: user && user.avatar !== ""
        ? user.avatar
        : (Style.darkMode ? "image://avatars/fallbackWhite" : "image://avatars/fallbackBlack")
    readonly property int maximumAccountTextWidth: Math.max(0,
                                                            Math.round(width * 0.55)
                                                            - Style.wizardHeaderAvatarSize
                                                            - Style.wizardHeaderRowSpacing)

    implicitHeight: Math.max(titleLabel.implicitHeight, accountRow.implicitHeight)

    EnforcedPlainTextLabel {
        id: titleLabel

        anchors.left: parent.left
        anchors.right: accountRow.visible ? accountRow.left : parent.right
        anchors.rightMargin: accountRow.visible ? Style.wizardHeaderSpacing : 0
        anchors.verticalCenter: parent.verticalCenter
        text: root.title
        color: Style.wizardPrimaryText
        font.pixelSize: Style.wizardHeaderTitleFontPixelSize
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignLeft
        elide: Text.ElideRight
    }

    RowLayout {
        id: accountRow

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: implicitWidth
        height: implicitHeight
        visible: root.user !== null
        spacing: Style.wizardHeaderRowSpacing

        Image {
            Layout.preferredWidth: Style.wizardHeaderAvatarSize
            Layout.preferredHeight: Style.wizardHeaderAvatarSize
            source: root.avatarSource
            sourceSize.width: Style.wizardHeaderAvatarSize
            sourceSize.height: Style.wizardHeaderAvatarSize
            fillMode: Image.PreserveAspectFit
            cache: false

            Accessible.role: Accessible.Graphic
            Accessible.name: qsTr("Account avatar")
        }

        ColumnLayout {
            Layout.preferredWidth: Math.min(root.maximumAccountTextWidth,
                                            Math.max(accountNameLabel.implicitWidth,
                                                     accountServerLabel.implicitWidth))
            Layout.maximumWidth: root.maximumAccountTextWidth
            Layout.minimumWidth: 0
            spacing: Style.wizardHeaderLabelSpacing

            EnforcedPlainTextLabel {
                id: accountNameLabel

                Layout.fillWidth: true
                text: root.user ? root.user.name : ""
                color: Style.wizardPrimaryText
                font.pixelSize: Style.wizardHeaderAccountNameFontPixelSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                id: accountServerLabel

                Layout.fillWidth: true
                text: root.user ? root.user.server : ""
                color: Style.wizardSecondaryText
                font.pixelSize: Style.wizardHeaderAccountServerFontPixelSize
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
            }
        }
    }
}
