/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style

Button {
    id: root
    objectName: "assistantTaskTypeDelegate"

    required property NC.AssistantController assistantController
    required property bool canUseAssistant
    required property string typeId
    required property string name
    required property bool isChat

    readonly property color idleBackgroundColor: {
        if (!enabled) {
            return Style.wizardDisabledButtonBackground
        }
        if (down) {
            return Style.wizardSecondaryButtonPressed
        }
        return hovered ? Style.wizardSecondaryButtonBackground : "transparent"
    }

    text: root.name
    checkable: true
    checked: root.assistantController.selectedTaskTypeId === root.typeId
    enabled: root.canUseAssistant && !root.assistantController.requestInProgress
    implicitHeight: Style.wizardFooterButtonHeight
    font.pixelSize: Style.wizardBodyFontPixelSize
    font.weight: checked ? Font.DemiBold : Font.Normal

    contentItem: Row {
        spacing: root.isChat ? Style.smallSpacing : 0

        Image {
            visible: root.isChat
            source: "image://svgimage-custom-color/comment.svg/"
                + (root.checked
                    ? Style.wizardSelectedText
                    : root.palette.buttonText)
            sourceSize.width: Style.smallIconSize
            sourceSize.height: Style.smallIconSize
            width: visible ? Style.smallIconSize : 0
            height: Style.smallIconSize
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }

        Text {
            text: root.text
            color: root.checked
                ? Style.wizardSelectedText
                : root.enabled
                    ? root.palette.buttonText
                    : Style.wizardDisabledText
            font: root.font
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        radius: Style.mediumRoundedButtonRadius
        border.width: root.activeFocus
            ? Style.thickBorderWidth
            : Style.normalBorderWidth
        border.color: root.checked || root.activeFocus
            ? Style.assistantSelectionGradientStart
            : root.hovered
                ? Style.wizardSecondaryButtonBorder
                : "transparent"

        gradient: Gradient {
            orientation: Gradient.Horizontal

            GradientStop {
                position: 0
                color: root.checked
                    ? Style.assistantSelectionGradientStart
                    : root.idleBackgroundColor
            }

            GradientStop {
                position: 1
                color: root.checked
                    ? Style.assistantSelectionGradientEnd
                    : root.idleBackgroundColor
            }
        }
    }

    Accessible.name: qsTr("Select assistant task type %1").arg(root.name)
    onClicked: root.assistantController.selectTaskType(root.typeId)
}
