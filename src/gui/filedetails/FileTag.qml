// SPDX-FileCopyrightText: 2023 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick

import Style
import "../tray"

EnforcedPlainTextLabel {
    id: internalLabel

    background: Rectangle {
        border.color: palette.dark
        border.width: Style.normalBorderWidth
        radius: Style.veryRoundedButtonRadius
        color: palette.base
    }

    elide: Text.ElideRight
    padding: Style.smallSpacing
}
