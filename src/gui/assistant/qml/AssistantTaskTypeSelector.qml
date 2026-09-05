/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls.Basic

import com.nextcloud.desktopclient as NC
import Style

ScrollView {
    id: root
    objectName: "assistantTaskTypeSelector"

    required property NC.AssistantController assistantController
    required property bool canUseAssistant

    // Temporarily hidden while task selection is not exposed in this iteration.
    // Keep this selector: it will be enabled and reused in a later iteration.
    visible: false
    clip: visible

    Row {
        spacing: Style.wizardFooterSpacing

        Repeater {
            model: root.visible ? root.assistantController.taskTypes : null

            delegate: Button {
                id: taskTypeButton

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

                text: name
                checkable: true
                checked: root.assistantController
                    && root.assistantController.selectedTaskTypeId === typeId
                enabled: root.canUseAssistant
                    && !root.assistantController.requestInProgress
                implicitHeight: Style.wizardFooterButtonHeight
                leftPadding: Style.wizardSectionSpacing
                rightPadding: Style.wizardSectionSpacing
                font.pixelSize: Style.wizardBodyFontPixelSize
                font.weight: checked ? Font.DemiBold : Font.Normal

                contentItem: Row {
                    spacing: taskTypeButton.isChat ? Style.smallSpacing : 0

                    Image {
                        visible: taskTypeButton.isChat
                        source: "image://svgimage-custom-color/comment.svg/"
                            + (taskTypeButton.checked
                                ? Style.wizardSelectedText
                                : taskTypeButton.palette.buttonText)
                        sourceSize.width: Style.smallIconSize
                        sourceSize.height: Style.smallIconSize
                        width: visible ? Style.smallIconSize : 0
                        height: Style.smallIconSize
                        anchors.verticalCenter: parent.verticalCenter
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        text: taskTypeButton.text
                        color: taskTypeButton.checked
                            ? Style.wizardSelectedText
                            : taskTypeButton.enabled
                                ? taskTypeButton.palette.buttonText
                                : Style.wizardDisabledText
                        font: taskTypeButton.font
                        anchors.verticalCenter: parent.verticalCenter
                        elide: Text.ElideRight
                    }
                }

                background: Rectangle {
                    radius: Style.mediumRoundedButtonRadius
                    border.width: taskTypeButton.activeFocus
                        ? Style.thickBorderWidth
                        : Style.normalBorderWidth
                    border.color: taskTypeButton.checked || taskTypeButton.activeFocus
                        ? Style.assistantSelectionGradientStart
                        : taskTypeButton.hovered
                            ? Style.wizardSecondaryButtonBorder
                            : "transparent"

                    gradient: Gradient {
                        orientation: Gradient.Horizontal

                        GradientStop {
                            position: 0
                            color: taskTypeButton.checked
                                ? Style.assistantSelectionGradientStart
                                : taskTypeButton.idleBackgroundColor
                        }

                        GradientStop {
                            position: 1
                            color: taskTypeButton.checked
                                ? Style.assistantSelectionGradientEnd
                                : taskTypeButton.idleBackgroundColor
                        }
                    }
                }

                Accessible.name: qsTr("Select assistant task type %1").arg(name)
                onClicked: root.assistantController.selectTaskType(typeId)

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    enabled: taskTypeButton.enabled
                    hoverEnabled: enabled
                    cursorShape: Qt.PointingHandCursor
                }
            }
        }
    }
}
