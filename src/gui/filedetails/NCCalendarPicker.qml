/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.nextcloud.desktopclient
import Style

Control {
    id: root

    signal userAcceptedDate

    function updateText() {
        dateDisplayLabel.text = backend.dateString;
    }

    DateFieldBackend {
        id: backend
        onDateStringChanged: if (!calendarPopup.opened) root.updateText()
    }

    property alias date: backend.date
    property alias dateInMs: backend.dateMsecs
    property alias minimumDate: backend.minimumDate
    property alias minimumDateMs: backend.minimumDateMsecs
    property alias maximumDate: backend.maximumDate
    property alias maximumDateMs: backend.maximumDateMsecs
    property alias validInput: backend.validDate

    implicitHeight: Math.max(Style.talkReplyTextFieldPreferredHeight, dateDisplayLabel.contentHeight + 16)

    background: Rectangle {
        color: palette.base
        border.color: root.enabled ? (root.hovered ? Style.ncBlue : palette.mid) : palette.mid
        border.width: 1
        radius: 4
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        Text {
            id: dateDisplayLabel
            Layout.fillWidth: true
            
            text: backend.dateString
            color: root.enabled ? palette.text : palette.placeholderText
            verticalAlignment: Text.AlignVCenter
        }

        Image {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            
            source: "image://svgimage-custom-color/calendar.svg/" + (root.enabled ? palette.text : palette.placeholderText)
            sourceSize.width: 20
            sourceSize.height: 20
            fillMode: Image.PreserveAspectFit
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        onClicked: calendarPopup.open()
    }

    Popup {
        id: calendarPopup
        
        x: 0
        y: parent.height + 4
        width: Math.max(300, parent.width)
        height: calendar.implicitHeight + 80
        
        padding: 12
        
        background: Rectangle {
            color: palette.window
            border.color: palette.mid
            border.width: 1
            radius: 8
            
            Rectangle {
                width: 12
                height: 12
                x: 20
                y: -6
                color: palette.window
                border.color: palette.mid
                border.width: 1
                rotation: 45
                z: -1
            }
        }
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 12
            
            RowLayout {
                Layout.fillWidth: true
                
                Button {
                    text: "◀"
                    onClicked: {
                        if (calendar.month > 0) {
                            calendar.month--
                        } else {
                            calendar.month = 11
                            calendar.year--
                        }
                    }
                }
                
                Text {
                    Layout.fillWidth: true
                    
                    text: Qt.locale().monthName(calendar.month) + " " + calendar.year
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    color: palette.text
                }
                
                Button {
                    text: "▶"
                    onClicked: {
                        if (calendar.month < 11) {
                            calendar.month++
                        } else {
                            calendar.month = 0
                            calendar.year++
                        }
                    }
                }
            }
            
            GridLayout {
                id: calendar
                
                Layout.fillWidth: true
                Layout.fillHeight: true
                
                columns: 7
                rowSpacing: 4
                columnSpacing: 4
                
                property int month: {
                    const date = new Date(backend.dateMsecs)
                    return date.getMonth()
                }
                property int year: {
                    const date = new Date(backend.dateMsecs)
                    return date.getFullYear()
                }
                
                // Day headers
                Repeater {
                    model: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]
                    
                    Text {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        
                        text: modelData
                        color: palette.text
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                
                // Calendar days
                Repeater {
                    model: calendarModel
                    
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        
                        property bool isCurrentMonth: modelData.month === calendar.month
                        property bool isToday: {
                            const today = new Date()
                            return modelData.date.getDate() === today.getDate() &&
                                   modelData.date.getMonth() === today.getMonth() &&
                                   modelData.date.getFullYear() === today.getFullYear()
                        }
                        property bool isSelected: {
                            const backendDate = new Date(backend.dateMsecs)
                            return modelData.date.getDate() === backendDate.getDate() &&
                                   modelData.date.getMonth() === backendDate.getMonth() &&
                                   modelData.date.getFullYear() === backendDate.getFullYear()
                        }
                        property bool isValidDate: {
                            const minDateMs = backend.minimumDateMsecs
                            const maxDateMs = backend.maximumDateMsecs
                            const currentDateMs = modelData.date.getTime()
                            
                            let valid = true
                            if (minDateMs > 0) {
                                valid = valid && currentDateMs >= minDateMs
                            }
                            if (maxDateMs > 0) {
                                valid = valid && currentDateMs <= maxDateMs
                            }
                            return valid
                        }
                        
                        color: {
                            if (!isCurrentMonth) return "transparent"
                            if (isSelected) return Style.ncBlue
                            if (mouseArea.containsMouse && isValidDate) return Qt.lighter(Style.ncBlue, 1.5)
                            if (isToday) return Qt.lighter(palette.highlight, 1.3)
                            return "transparent"
                        }
                        
                        radius: 4
                        
                        Text {
                            anchors.centerIn: parent
                            text: modelData.date.getDate()
                            color: {
                                if (!parent.isCurrentMonth) return palette.placeholderText
                                if (!parent.isValidDate) return palette.placeholderText
                                if (parent.isSelected) return "white"
                                if (parent.isToday) return palette.highlightedText
                                return palette.text
                            }
                            font.bold: parent.isToday
                        }
                        
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            enabled: parent.isValidDate && parent.isCurrentMonth
                            
                            onClicked: {
                                backend.dateMsecs = modelData.date.getTime()
                                root.userAcceptedDate()
                                calendarPopup.close()
                            }
                        }
                    }
                }
            }
            
            property var calendarModel: {
                const result = []
                const firstDay = new Date(calendar.year, calendar.month, 1)
                const lastDay = new Date(calendar.year, calendar.month + 1, 0)
                const startDate = new Date(firstDay)
                startDate.setDate(startDate.getDate() - firstDay.getDay())
                
                for (let i = 0; i < 42; i++) { // 6 weeks × 7 days
                    const currentDate = new Date(startDate)
                    currentDate.setDate(startDate.getDate() + i)
                    result.push({
                        date: currentDate,
                        month: currentDate.getMonth()
                    })
                }
                return result
            }
            
            RowLayout {
                Layout.fillWidth: true
                
                Button {
                    text: qsTr("Today")
                    onClicked: {
                        const today = new Date()
                        calendar.month = today.getMonth()
                        calendar.year = today.getFullYear()
                        backend.dateMsecs = today.getTime()
                        root.userAcceptedDate()
                        calendarPopup.close()
                    }
                }
                
                Item { Layout.fillWidth: true }
                
                Button {
                    text: qsTr("Cancel")
                    onClicked: calendarPopup.close()
                }
            }
        }
    }
}