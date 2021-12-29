import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.15

import Style 1.0

import com.nextcloud.desktopclient 1.0 as NC

RowLayout {
    id: layout

    property alias model: syncStatus

    spacing: 0

    NC.SyncStatusSummary {
        id: syncStatus
    }

    Image {
        id: syncIcon

        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
        Layout.topMargin: 16
        Layout.bottomMargin: 16
        Layout.leftMargin: 16

        source: syncStatus.syncIcon
        sourceSize.width: 32
        sourceSize.height: 32
        rotation: syncStatus.syncing ? 0 : 0
    }
 
    RotationAnimator {
        target: syncIcon
        running:  syncStatus.syncing
        from: 0
        to: 360
        loops: Animation.Infinite
        duration: 3000
    }

    ColumnLayout {
        id: syncProgressLayout

        Layout.alignment: Qt.AlignVCenter
        Layout.topMargin: 8
        Layout.rightMargin: 16
        Layout.leftMargin: 10
        Layout.bottomMargin: 8
        Layout.fillWidth: true
        Layout.fillHeight: true

        Text {
            id: syncProgressText
            
            Layout.fillWidth: true

            text: syncStatus.syncStatusString
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Style.topLinePixelSize
            font.bold: true
        }

        Loader {
            Layout.fillWidth: true

            active: syncStatus.syncing;
            visible: syncStatus.syncing
            
            sourceComponent: ProgressBar {
                id: syncProgressBar

                value: syncStatus.syncProgress
            }
        }

        Text {
            id: syncProgressDetailText

            Layout.fillWidth: true

            text: syncStatus.syncStatusDetailString
            visible: syncStatus.syncStatusDetailString !== ""
            color: "#808080"
            font.pixelSize: Style.subLinePixelSize
        }
    }
}
