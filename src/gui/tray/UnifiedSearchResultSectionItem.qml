/*
 * Copyright (C) 2021 by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

EnforcedPlainTextLabel {
    required property string section

    topPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    bottomPadding: Style.unifiedSearchResultSectionItemVerticalPadding
    leftPadding: Style.unifiedSearchResultSectionItemLeftPadding

    text: section
    font.pixelSize: Style.unifiedSearchResultTitleFontSize
    color: UserModel.currentUser.accentColor

    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Search results section %1").arg(section)
}
