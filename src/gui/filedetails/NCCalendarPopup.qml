/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import com.nextcloud.desktopclient
import Style

Popup {
    id: root

    property date selectedDate: new Date()
    property date minimumDate: new Date(1900, 0, 1)
    property date maximumDate: new Date(2100, 11, 31)

    property int displayMonth: selectedDate.getMonth()
    property int displayYear: selectedDate.getFullYear()

    signal dateSelected(date selectedDate)

    width: 280
    height: 320
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: palette.base
        border.color: palette.mid
        border.width: 1
        radius: Style.slightlyRoundedButtonRadius

        Rectangle {
            anchors.fill: parent
            anchors.margins: 1
            color: "transparent"
            border.color: palette.highlight
            border.width: 1
            radius: Style.slightlyRoundedButtonRadius
            opacity: 0.3
        }
    }

    function monthName(month) {
        const monthNames = [
            qsTr("January"), qsTr("February"), qsTr("March"), qsTr("April"),
            qsTr("May"), qsTr("June"), qsTr("July"), qsTr("August"),
            qsTr("September"), qsTr("October"), qsTr("November"), qsTr("December")
        ];
        return monthNames[month];
    }

    function dayName(day) {
        const dayNames = [qsTr("Sun"), qsTr("Mon"), qsTr("Tue"), qsTr("Wed"), qsTr("Thu"), qsTr("Fri"), qsTr("Sat")];
        return dayNames[day];
    }

    function daysInMonth(month, year) {
        return new Date(year, month + 1, 0).getDate();
    }

    function firstDayOfMonth(month, year) {
        return new Date(year, month, 1).getDay();
    }

    function isDateValid(date) {
        return date >= minimumDate && date <= maximumDate;
    }

    function isDateSelected(date) {
        return selectedDate && 
               date.getDate() === selectedDate.getDate() &&
               date.getMonth() === selectedDate.getMonth() &&
               date.getFullYear() === selectedDate.getFullYear();
    }

    function isToday(date) {
        const today = new Date();
        return date.getDate() === today.getDate() &&
               date.getMonth() === today.getMonth() &&
               date.getFullYear() === today.getFullYear();
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        spacing: Style.standardSpacing

        // Month/Year navigation
        RowLayout {
            Layout.fillWidth: true

            Button {
                id: previousButton
                text: "‹"
                font.pixelSize: Style.fontPixelSizeResolveFont * 1.2
                onClicked: {
                    if (displayMonth === 0) {
                        displayMonth = 11
                        displayYear--
                    } else {
                        displayMonth--
                    }
                }
                background: Rectangle {
                    color: parent.hovered ? palette.button : "transparent"
                    radius: Style.slightlyRoundedButtonRadius
                }
            }

            Text {
                Layout.fillWidth: true
                text: monthName(displayMonth) + " " + displayYear
                font.pixelSize: Style.fontPixelSizeResolveFont
                font.bold: true
                color: palette.windowText
                horizontalAlignment: Text.AlignHCenter
            }

            Button {
                id: nextButton
                text: "›"
                font.pixelSize: Style.fontPixelSizeResolveFont * 1.2
                onClicked: {
                    if (displayMonth === 11) {
                        displayMonth = 0
                        displayYear++
                    } else {
                        displayMonth++
                    }
                }
                background: Rectangle {
                    color: parent.hovered ? palette.button : "transparent"
                    radius: Style.slightlyRoundedButtonRadius
                }
            }
        }

        // Day of week headers
        GridLayout {
            Layout.fillWidth: true
            columns: 7
            rowSpacing: 2
            columnSpacing: 2

            Repeater {
                model: 7
                Text {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    text: dayName(index)
                    font.pixelSize: Style.fontPixelSizeResolveFont * 0.8
                    color: palette.placeholderText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Calendar grid
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 7
            rowSpacing: 2
            columnSpacing: 2

            Repeater {
                model: 42 // 6 weeks * 7 days

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    property int dayNumber: {
                        const firstDay = firstDayOfMonth(displayMonth, displayYear);
                        const daysInCurrentMonth = daysInMonth(displayMonth, displayYear);
                        const dayIndex = index - firstDay + 1;
                        
                        if (dayIndex <= 0) {
                            // Previous month
                            const prevMonth = displayMonth === 0 ? 11 : displayMonth - 1;
                            const prevYear = displayMonth === 0 ? displayYear - 1 : displayYear;
                            return daysInMonth(prevMonth, prevYear) + dayIndex;
                        } else if (dayIndex > daysInCurrentMonth) {
                            // Next month
                            return dayIndex - daysInCurrentMonth;
                        } else {
                            // Current month
                            return dayIndex;
                        }
                    }
                    
                    property bool isCurrentMonth: {
                        const firstDay = firstDayOfMonth(displayMonth, displayYear);
                        const daysInCurrentMonth = daysInMonth(displayMonth, displayYear);
                        const dayIndex = index - firstDay + 1;
                        return dayIndex > 0 && dayIndex <= daysInCurrentMonth;
                    }
                    
                    property date cellDate: {
                        const firstDay = firstDayOfMonth(displayMonth, displayYear);
                        const dayIndex = index - firstDay + 1;
                        
                        if (dayIndex <= 0) {
                            // Previous month
                            const prevMonth = displayMonth === 0 ? 11 : displayMonth - 1;
                            const prevYear = displayMonth === 0 ? displayYear - 1 : displayYear;
                            return new Date(prevYear, prevMonth, dayNumber);
                        } else if (dayIndex > daysInMonth(displayMonth, displayYear)) {
                            // Next month
                            const nextMonth = displayMonth === 11 ? 0 : displayMonth + 1;
                            const nextYear = displayMonth === 11 ? displayYear + 1 : displayYear;
                            return new Date(nextYear, nextMonth, dayNumber);
                        } else {
                            // Current month
                            return new Date(displayYear, displayMonth, dayNumber);
                        }
                    }
                    
                    property bool isValidDate: root.isDateValid(cellDate)
                    property bool isSelected: root.isDateSelected(cellDate)
                    property bool todayDate: root.isToday(cellDate)

                    color: {
                        if (!isCurrentMonth) return "transparent"
                        if (isSelected) return palette.highlight
                        if (todayDate) return palette.button
                        if (mouseArea.containsMouse) return palette.alternateBase
                        return "transparent"
                    }

                    radius: Style.slightlyRoundedButtonRadius
                    border.width: todayDate && !isSelected ? 1 : 0
                    border.color: palette.highlight

                    Text {
                        anchors.centerIn: parent
                        text: parent.dayNumber
                        font.pixelSize: Style.fontPixelSizeResolveFont * 0.9
                        color: {
                            if (!parent.isCurrentMonth) return palette.placeholderText
                            if (!parent.isValidDate) return palette.placeholderText
                            if (parent.isSelected) return palette.highlightedText
                            return palette.windowText
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: parent.isValidDate && parent.isCurrentMonth

                        onClicked: {
                            root.selectedDate = parent.cellDate
                            root.dateSelected(parent.cellDate)
                            root.close()
                        }
                    }
                }
            }
        }

        // Action buttons
        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("Today")
                enabled: {
                    const today = new Date()
                    return root.isDateValid(today)
                }
                onClicked: {
                    const today = new Date()
                    root.selectedDate = today
                    root.displayMonth = today.getMonth()
                    root.displayYear = today.getFullYear()
                    root.dateSelected(today)
                    root.close()
                }
                background: Rectangle {
                    color: parent.hovered ? palette.button : "transparent"
                    border.color: palette.mid
                    border.width: 1
                    radius: Style.slightlyRoundedButtonRadius
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("Cancel")
                onClicked: root.close()
                background: Rectangle {
                    color: parent.hovered ? palette.button : "transparent"
                    border.color: palette.mid
                    border.width: 1
                    radius: Style.slightlyRoundedButtonRadius
                }
            }
        }
    }

    onOpened: {
        // Set the calendar to show the month of the selected date
        if (selectedDate) {
            displayMonth = selectedDate.getMonth()
            displayYear = selectedDate.getFullYear()
        }
    }
}