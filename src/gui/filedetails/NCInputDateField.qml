/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import com.nextcloud.desktopclient
import Style

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

    // Increase right padding to make room for calendar button
    rightPadding: submitButton.width + calendarButton.width

    onAccepted: {
        backend.dateString = text;
        root.userAcceptedDate();
    }

    Button {
        id: calendarButton
        
        anchors.top: root.top
        anchors.right: submitButton.left
        anchors.margins: 1
        
        width: height
        height: parent.height
        
        background: null
        flat: true
        icon.source: "image://svgimage-custom-color/calendar.svg/" + root.secondaryColor
        icon.color: hovered && enabled ? UserModel.currentUser.accentColor : root.secondaryColor
        
        enabled: root.enabled
        
        onClicked: {
            calendarPopup.selectedDate = backend.date.valueOf() ? backend.date : new Date()
            calendarPopup.open()
        }
        
        ToolTip.visible: hovered && enabled
        ToolTip.text: qsTr("Open calendar")
        ToolTip.delay: 1000
    }

    NCCalendarPopup {
        id: calendarPopup
        
        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)
        
        minimumDate: backend.minimumDate
        maximumDate: backend.maximumDate
        
        onDateSelected: function(selectedDate) {
            backend.setDate(selectedDate)
            root.userAcceptedDate()
        }
    }
}

