/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import Style
import "qrc:/qml/src/gui/tray"

Item {
    id: root

    required property var searchModel

    signal navigateBack()

    objectName: "searchDetailHeader"
    implicitHeight: Style.unifiedSearchDetailHeaderHeight

    ToolButton {
        id: backButton

        objectName: "searchDetailBackButton"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Back")
        icon.source: "image://svgimage-custom-color/"
            + (root.LayoutMirroring.enabled ? "arrow-right.svg/" : "arrow-left.svg/")
            + Style.wizardPrimaryText
        icon.width: Style.smallIconSize
        icon.height: Style.smallIconSize
        display: AbstractButton.TextBesideIcon
        Accessible.name: qsTr("Back to all search results")
        onClicked: {
            root.searchModel.closeProviderDetail()
            root.navigateBack()
        }
    }

    EnforcedPlainTextLabel {
        objectName: "searchDetailProviderTitle"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: backButton.width + Style.smallSpacing
        anchors.rightMargin: backButton.width + Style.smallSpacing
        anchors.verticalCenter: parent.verticalCenter
        text: root.searchModel ? root.searchModel.detailProviderName : ""
        font.bold: true
        font.pixelSize: Style.wizardHeaderTitleFontPixelSize
        elide: Text.ElideRight
        horizontalAlignment: Text.AlignHCenter
    }
}
