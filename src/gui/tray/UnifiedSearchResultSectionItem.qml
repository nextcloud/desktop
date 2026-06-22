/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import com.nextcloud.desktopclient

EnforcedPlainTextLabel {
    required property string section

    topPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    bottomPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    leftPadding: Style.unifiedSearchResultSectionItemLeftPadding

    text: section
    font.pixelSize: Style.unifiedSearchResultTitleFontSize

    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Search results section %1").arg(section)
}
