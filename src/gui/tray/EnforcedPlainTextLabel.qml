/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import QtQuick
import QtQuick.Controls

Label {
    function resetToPlainText() {
        if (textFormat !== Text.PlainText) {
            console.log("WARNING: this label was set to a non-plain text format. Resetting to plain text.")
            textFormat = Text.PlainText;
        }
    }

    textFormat: Text.PlainText
    onTextFormatChanged: resetToPlainText()
    Component.onCompleted: resetToPlainText()
}
