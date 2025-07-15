import QtQuick 2.15
import QtQuick.Layouts
import QtQuick.Controls

import com.nextcloud.desktopclient

Rectangle {
    id: root
    readonly property color borderColor: Qt.rgba(0, 0.51, 0.79, 1)
    readonly property color ncBlue: ClientTheme.wizardHeaderBackgroundColor
    readonly property color ncHeaderTextColor: ClientTheme.wizardHeaderTitleColor
    readonly property color textColor: Qt.darker(ncHeaderTextColor, 4)
    readonly property color bkgColor: ClientTheme.darkMode ? Qt.lighter(ncBlue, 2)
                                                            : Qt.darker(ncBlue, 1.5)
    readonly property color accentColor: ClientTheme.darkMode ? Qt.lighter(borderColor, 2)
                                                            : Qt.darker(borderColor, 1.5)
    readonly property int titleSize: 20
    readonly property int subTitleSize: 14
    readonly property int textSize: 12
    readonly property int spacing: 10
    readonly property int smallSpacing: 5
    readonly property int radius: 20
    readonly property int margins: 20
    readonly property int bigMargins: 60
    readonly property int smallMargins: 15
    readonly property int boxWidth: 160
    readonly property int boxHeight: 190
    readonly property int radioButtonSize: 10

    color: root.ncBlue
    width: 500
    height: 500

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.bigMargins

        Label {
            text: qsTr("Select the client mode")
            color: root.ncHeaderTextColor
            font.pixelSize: root.titleSize
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }

        Label {
            id: description
            text: qsTr("Nextcloud offers sync and on-demand file access.\nThis setting applies for all accounts.")
            font.pixelSize: root.titleSize
            color: root.ncHeaderTextColor
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            Layout.alignment: Qt.AlignHCenter
        }

        RowLayout {
            spacing: root.margins
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            Layout.topMargin: root.smallSpacing

            // Classic Sync option
            Rectangle {
                width: root.boxWidth
                height: root.boxHeight
                radius: root.radius
                border.color: root.borderColor
                color: root.ncHeaderTextColor

                RadioButton {
                    id: classicSync
                    anchors.top: parent.top
                    anchors.topMargin: root.smallMargins
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.radioButtonSize
                    height: root.radioButtonSize
                }

                Column {
                    anchors.top: classicSync.bottom
                    anchors.topMargin: root.smallMargins
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: root.spacing

                    Text {
                        text: qsTr("Classic Sync")
                        font.bold: true
                        color: root.textColor
                        font.pixelSize: root.subTitleSize
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: qsTr("The folders to be\nsynced can be selected.\n\nThe files will always\nbe kept on this device.")
                        font.pixelSize: root.textSize
                        color: root.textColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            // Virtual Files option
            Rectangle {
                width: root.boxWidth
                height: root.boxHeight
                radius: root.radius
                border.color: root.borderColor
                color: root.ncHeaderTextColor

                RadioButton {
                    id: virtualFiles
                    anchors.top: parent.top
                    anchors.topMargin: root.smallMargins
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.radioButtonSize
                    height: root.radioButtonSize
                }

                Column {
                    anchors.top: virtualFiles.bottom
                    anchors.topMargin: root.smallMargins
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: root.spacing

                    Text {
                        text: qsTr("Virtual Files")
                        color: root.textColor
                        font.bold: true
                        font.pixelSize: root.subTitleSize
                        horizontalAlignment: Text.AlignHCenter
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text {
                        text: qsTr("All folders are locally\navailable.\n\nFiles are downloaded\non demand. They free\nup disk space when\nnot in use.")
                        font.pixelSize: root.textSize
                        color: root.textColor
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
}
