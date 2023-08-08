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

import QtQuick 2.15
import QtQuick.Controls 2.15
import com.nextcloud.desktopclient 1.0

NCInputTextField {
    id: root

    signal userAcceptedDate

    function updateText() {
        text = backend.dateTimeString;
    }

    property DateFieldBackend backend: DateFieldBackend { 
        id: backend
        onDateTimeStringChanged: if (!root.activeFocus) root.updateText()
    }

    property alias date: backend.dateTime
    property alias dateInMs: backend.dateTimeMsecs
    property alias minimumDate: backend.minimumDateTime
    property alias minimumDateMs: backend.minimumDateTimeMsecs
    property alias maximumDate: backend.maximumDateTime
    property alias maximumDateMs: backend.maximumDateTimeMsecs

    validInput: backend.validDateTime
    text: backend.dateTimeString
    onTextChanged: backend.dateTimeString = text

    onAccepted: {
        backend.dateTimeString = text;
        root.userAcceptedDate();
    }
}