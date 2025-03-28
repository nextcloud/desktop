// SPDX-FileCopyrightText: 2023 Claudio Cambra <claudio.cambra@nextcloud.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

