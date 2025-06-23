/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient
import Style
import "../tray"
import "../"

ColumnLayout {
    id: root

    property string localPath: ""
    property var accountState: ({})
    property FileDetails fileDetails: FileDetails {}
    property int horizontalPadding: 0
    property int iconSize: 32
    property bool backgroundsVisible: true
    property color accentColor: Style.ncBlue
    property StackView rootStackView: StackView {}

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
