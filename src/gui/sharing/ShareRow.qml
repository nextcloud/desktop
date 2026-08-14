/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "qrc:/qml/src/gui/wizard/qml"

WizardItemDelegate {
    id: root

    required property Share share
    required property string recipientNames
    signal configureRequested

    readonly property bool pending: share.state === Share.Draft

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
                text: root.recipientNames || (root.pending ? qsTr("Unfinished share") : qsTr("Share"))
                color: Style.wizardPrimaryText
                elide: Text.ElideRight
            }

            EnforcedPlainTextLabel {
                Layout.fillWidth: true
                text: root.pending
                    ? qsTr("Not active — select to finish")
                    : qsTr("%n recipient(s)", "", root.share.recipients.length)
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
            iconSource: "image://svgimage-custom-color/more.svg/" + palette.buttonText
            Accessible.name: qsTr("Configure share")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name

            onClicked: root.configureRequested()
        }
    }
}
