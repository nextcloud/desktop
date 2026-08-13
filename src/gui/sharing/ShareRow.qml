/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style

ItemDelegate {
    id: root

    required property Share share
    signal configureRequested

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    hoverEnabled: true

    contentItem: RowLayout {
        spacing: Style.smallSpacing

        Image {
            Layout.preferredWidth: Style.activityListButtonIconSize
            Layout.preferredHeight: Style.activityListButtonIconSize
            source: "image://svgimage-custom-color/share.svg/" + palette.buttonText
            sourceSize: Qt.size(Style.activityListButtonIconSize, Style.activityListButtonIconSize)
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: {
                    const names = []
                    if (root.share && root.share.recipients) {
                        for (const recipient of Array.from(root.share.recipients)) {
                            if (recipient && recipient.displayName) {
                                names.push(recipient.displayName)
                            }
                        }
                    }
                    return names.length > 0 ? names.join(", ") : qsTr("New share")
                }
                color: palette.text
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: root.share && root.share.recipients && root.share.recipients.length > 0 ? qsTr("%1 recipient(s)").arg(root.share.recipients.length) : qsTr("Not shared yet")
                color: palette.placeholderText
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        ToolButton {
            Layout.preferredWidth: Style.activityListButtonWidth
            Layout.preferredHeight: Style.activityListButtonHeight
            display: AbstractButton.IconOnly
            icon.source: "image://svgimage-custom-color/more.svg/" + palette.buttonText
            Accessible.name: qsTr("Configure share")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name

            onClicked: root.configureRequested()
        }
    }
}
