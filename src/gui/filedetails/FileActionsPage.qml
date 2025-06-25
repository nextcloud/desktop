/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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

Page {
    id: root

    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue

    property FileDetails fileDetails: FileDetails {}
    property var shareModelData: ({})
    property StackView rootStackView: StackView {}

    padding: Style.standardSpacing * 2

    background: Rectangle {
        color: palette.base
        visible: root.backgroundsVisible
    }

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: root.horizontalPadding
        Layout.rightMargin: root.horizontalPadding

        ColumnLayout {
            EnforcedPlainTextLabel {
                text: root.localPath;
            }

            EnforcedPlainTextLabel {
                text: qsTr("TBD");
            }
        }
    }
}
