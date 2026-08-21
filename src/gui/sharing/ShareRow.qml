/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui/tray"
import "qrc:/qml/src/gui/wizard/qml"

WizardItemDelegate {
    id: root

    property var share: null
    property string recipientNames: ""
    property bool publicLink: false
    property string publicLinkUrl: ""
    signal copyRequested
    signal configureRequested

    readonly property bool pending: !!root.share && root.share.state === Share.Draft

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    contentItem: RowLayout {
        spacing: Style.standardSpacing

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
                text: root.publicLink ? qsTr("Share link") : root.recipientNames || (root.pending ? qsTr("Unfinished share") : qsTr("Share"))
                color: Style.wizardPrimaryText
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: {
                    if (root.pending) {
                        return qsTr("Not active — select to finish")
                    }
                    if (root.publicLink && root.share) {
                        return root.share.permissionPresetLabel || ""
                    }
                    return root.share && root.share.recipients ? qsTr("%n recipient(s)", "", root.share.recipients.length) : ""
                }
                color: Style.wizardSecondaryText
                elide: Text.ElideRight
                visible: text.length > 0
            }
        }

        WizardButton {
            Layout.preferredWidth: implicitHeight
            leftPadding: 0
            rightPadding: 0
            text: ""
            iconSource: "image://svgimage-custom-color/copy.svg/" + palette.buttonText
            visible: root.publicLink && root.publicLinkUrl.length > 0
            enabled: visible

            Accessible.name: qsTr("Copy public link")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name

            onClicked: root.copyRequested()
        }

        WizardButton {
            Layout.preferredWidth: implicitHeight
            leftPadding: 0
            rightPadding: 0
            text: ""
            iconSource: "image://svgimage-custom-color/more.svg/" + palette.buttonText
            Accessible.name: qsTr("Configure share")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name

            onClicked: root.configureRequested()
        }
    }
}
