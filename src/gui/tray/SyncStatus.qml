import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Style 1.0

import com.nextcloud.desktopclient 1.0 as NC

RowLayout {
    id: root

    property alias model: syncStatus

    spacing: Style.trayHorizontalMargin

    NC.SyncStatusSummary {
        id: syncStatus
    }

    NCBusyIndicator {
        id: syncIcon
        property int size: Style.trayListItemIconSize * 0.6
        property int whiteSpace: (Style.trayListItemIconSize - size)

        Layout.preferredWidth: size
        Layout.preferredHeight: size

        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        Layout.topMargin: 16
        Layout.rightMargin: whiteSpace * (0.5 + Style.thumbnailImageSizeReduction)
        Layout.bottomMargin: 16
        Layout.leftMargin: Style.trayHorizontalMargin + (whiteSpace * (0.5 - Style.thumbnailImageSizeReduction))

        padding: 0

        imageSource: syncStatus.syncIcon
        running: syncStatus.syncing
    }

    ColumnLayout {
        id: syncProgressLayout

        Layout.alignment: Qt.AlignVCenter
        Layout.topMargin: 8
        Layout.rightMargin: Style.trayHorizontalMargin
        Layout.bottomMargin: 8
        Layout.fillWidth: true
        Layout.fillHeight: true

        EnforcedPlainTextLabel {
            id: syncProgressText

            Layout.fillWidth: true

            text: syncStatus.syncStatusString
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Style.topLinePixelSize
            font.bold: true
            wrapMode: Text.Wrap
        }

        Loader {
            Layout.fillWidth: true

            active: syncStatus.syncing
            visible: syncStatus.syncing

            sourceComponent: ProgressBar {
                id: syncProgressBar

                // TODO: Rather than setting all these palette colours manually,
                // create a custom style and do it for all components globally
                palette.window: palette.dark // NOTE: Fusion theme uses darker window colour for the border of the progress bar
                value: syncStatus.syncProgress
            }
        }

        EnforcedPlainTextLabel {
            id: syncProgressDetailText

            Layout.fillWidth: true

            text: syncStatus.syncStatusDetailString
            visible: syncStatus.syncStatusDetailString !== ""
            color: palette.midlight
            font.pixelSize: Style.subLinePixelSize
            wrapMode: Text.Wrap
        }
    }

    CustomButton {
        id: syncNowButton

        FontMetrics {
            id: syncNowFm
            font: syncNowButton.contentsFont
        }

        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Sync now")

        padding: Style.smallSpacing
        textColor: Style.adjustedCurrentUserHeaderColor
        textColorHovered: Style.currentUserHeaderTextColor
        contentsFont.bold: true
        bgColor: Style.currentUserHeaderColor

        visible: !activityModel.hasSyncConflicts &&
                 !syncStatus.syncing &&
                 NC.UserModel.currentUser.hasLocalFolder &&
                 NC.UserModel.currentUser.isConnected
        enabled: visible
        onClicked: {
            if(!syncStatus.syncing) {
                NC.UserModel.currentUser.forceSyncNow();
            }
        }
    }

    CustomButton {
        Layout.preferredWidth: syncNowFm.boundingRect(text).width +
                               leftPadding +
                               rightPadding +
                               Style.standardSpacing * 2
        Layout.rightMargin: Style.trayHorizontalMargin

        text: qsTr("Resolve conflicts")
        textColor: Style.adjustedCurrentUserHeaderColor
        textColorHovered: Style.currentUserHeaderTextColor
        contentsFont.bold: true
        bgColor: Style.currentUserHeaderColor

        visible: activityModel.hasSyncConflicts &&
                 !syncStatus.syncing &&
                 NC.UserModel.currentUser.hasLocalFolder &&
                 NC.UserModel.currentUser.isConnected
        enabled: visible
        onClicked: NC.Systray.createResolveConflictsDialog(activityModel.allConflicts);
    }
}
