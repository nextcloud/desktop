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
import com.nextcloud.desktopclient

NCInputTextField {
    id: root

    signal userAcceptedDate

    function updateText() {
        text = backend.dateString;
    }

    DateFieldBackend {
        id: backend
        onDateStringChanged: if (!root.activeFocus) root.updateText()
    }

    property alias date: backend.date
    property alias dateInMs: backend.dateMsecs
    property alias minimumDate: backend.minimumDate
    property alias minimumDateMs: backend.minimumDateMsecs
    property alias maximumDate: backend.maximumDate
    property alias maximumDateMs: backend.maximumDateMsecs

    inputMethodHints: Qt.ImhDate
    validInput: backend.validDate
    text: backend.dateString
    onTextChanged: backend.dateString = text

    onAccepted: {
        backend.dateString = text;
        root.userAcceptedDate();
    }
}

