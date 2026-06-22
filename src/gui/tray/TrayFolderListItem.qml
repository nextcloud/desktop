/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style

MenuItem {
    id: root

    property string subline: ""
    property string iconSource: "image://svgimage-custom-color/account-group.svg/" + palette.buttonText
    property string backgroundIconSource: value
    property string toolTipText: root.text

    ToolTip {
        popupType: Qt.platform.os === "windows" ? Popup.Item : Popup.Native
        visible: root.hovered && root.toolTipText !== ""
        text: root.toolTipText
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Style.trayWindowMenuEntriesMargin
        anchors.rightMargin: Style.trayWindowMenuEntriesMargin
        spacing: Style.trayHorizontalMargin

        NCIconWithBackgroundImage {
            source: root.backgroundIconSource

            icon.source: root.iconSource
            icon.height: height * Style.smallIconScaleFactor
            icon.width: icon.height

            Layout.preferredHeight: root.height * Style.smallIconScaleFactor
            Layout.preferredWidth: root.height * Style.smallIconScaleFactor
            Layout.alignment: Qt.AlignVCenter
        }

        ListItemLineAndSubline {
            lineText: root.text
            sublineText: root.subline

            spacing: Style.extraSmallSpacing

            Layout.alignment: Qt.AlignVCenter

            Layout.fillWidth: true

        }
    }
}
