import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

Item {
    id: root

    property variant activityData: {{}}

    property variant activity: {{}}

    property color activityTextTitleColor: Style.ncTextColor

    property bool showDismissButton: false

    property bool childHovered: fileDetailsButton.hovered || dismissActionButton.hovered

    property int iconSize: Style.trayListItemIconSize

    signal dismissButtonClicked()

    Item {
        id: thumbnailItem

        readonly property int imageWidth: width * (1 - Style.thumbnailImageSizeReduction)
        readonly property int imageHeight: height * (1 - Style.thumbnailImageSizeReduction)
        readonly property int thumbnailRadius: model.thumbnail && model.thumbnail.isUserAvatar ? width / 2 : 3

        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        implicitHeight: model.thumbnail && model.thumbnail.isMimeTypeIcon ? root.iconSize * 0.9 : root.iconSize
        implicitWidth: root.iconSize

        Loader {
            id: thumbnailImageLoader
            anchors.fill: parent
            active: model.thumbnail !== undefined

            sourceComponent: Item {
                anchors.fill: parent
                readonly property int paintedWidth: model.thumbnail.isMimeTypeIcon ? thumbnailImage.paintedWidth * 0.8 : thumbnailImage.paintedWidth
                readonly property int paintedHeight: model.thumbnail.isMimeTypeIcon ? thumbnailImage.paintedHeight * 0.55 : thumbnailImage.paintedHeight

                Image {
                    id: thumbnailImage
                    width: thumbnailItem.imageWidth
                    height: thumbnailItem.imageHeight
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    cache: true
                    fillMode: Image.PreserveAspectFit
                    source: model.thumbnail.source
                    visible: false
                    sourceSize.height: 64
                    sourceSize.width: 64
                }

                Rectangle {
                    id: mask
                    color: "white"
                    radius: thumbnailItem.thumbnailRadius
                    anchors.fill: thumbnailImage
                    visible: false
                    width: thumbnailImage.paintedWidth
                    height: thumbnailImage.paintedHeight
                }

                OpacityMask {
                    anchors.fill: thumbnailImage
                    source: thumbnailImage
                    maskSource: mask
                    visible: model.thumbnail !== undefined
                }
            }
        }

        Image {
            id: activityIcon
            width: model.thumbnail !== undefined ? parent.width * 0.4 : thumbnailItem.imageWidth
            height: model.thumbnail !== undefined ? width : width * 0.9

            // Prevent bad access into unloaded item properties
            readonly property int thumbnailPaintedWidth: thumbnailImageLoader.item ? thumbnailImageLoader.item.paintedWidth : 0
            readonly property int thumbnailPaintedHeight: thumbnailImageLoader.item ? thumbnailImageLoader.item.paintedHeight : 0

            readonly property int negativeLeftMargin: -((width / 2) +
                                                        ((width - paintedWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailItem.imageWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailPaintedWidth) / 2) +
                                                        (thumbnailItem.thumbnailRadius / 4))
            readonly property int negativeTopMargin: -((height / 2) +
                                                       ((height - paintedHeight) / 2) +
                                                       ((thumbnailImageLoader.height - thumbnailItem.imageHeight) / 4) +
                                                       ((thumbnailImageLoader.height - thumbnailPaintedHeight) / 4) +
                                                       (thumbnailItem.thumbnailRadius / 4))

            anchors.verticalCenter: if(model.thumbnail === undefined) parent.verticalCenter
            anchors.left: model.thumbnail === undefined ? parent.left : thumbnailImageLoader.right
            anchors.leftMargin: if(model.thumbnail !== undefined) negativeLeftMargin
            anchors.top: if(model.thumbnail !== undefined) thumbnailImageLoader.bottom
            anchors.topMargin: if(model.thumbnail !== undefined) negativeTopMargin

            cache: true
            fillMode: Image.PreserveAspectFit
            source: Theme.darkMode ? model.darkIcon : model.lightIcon
            sourceSize.height: 64
            sourceSize.width: 64
            mipmap: true // Addresses grainy downscale
        }
    }

    ColumnLayout {
        id: activityContentLayout

        anchors.left: thumbnailItem.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        spacing: Style.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: activityContentLayout.width

            spacing: Style.trayHorizontalMargin

            EnforcedPlainTextLabel {
                id: activityTextTitle
                text: (root.activityData.type === "Activity" || root.activityData.type === "Notification") ? root.activityData.subject : root.activityData.message
                height: (text === "") ? 0 : implicitHeight

                Layout.maximumWidth: activityContentLayout.width - Style.trayHorizontalMargin -
                                     (activityTextDateTime.visible ? activityTextDateTime.width + Style.trayHorizontalMargin : 0) -
                                     (dismissActionButton.visible ? dismissActionButton.width + Style.trayHorizontalMargin : 0)
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 1
                font.pixelSize: Style.topLinePixelSize
                color: Style.ncTextColor
                visible: text !== ""

                NCToolTip {
                    text: parent.text
                    visible: parent.hovered
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.leftMargin: -Style.trayHorizontalMargin
            }

            EnforcedPlainTextLabel {
                id: activityTextDateTime

                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                height: (text === "") ? 0 : implicitHeight
                width: parent.width

                text: root.activityData.dateTime
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.subLinePixelSize
                color: Style.ncSecondaryTextColor
                visible: text !== ""
            }

            RoundButton {
                id: dismissActionButton

                Layout.preferredWidth: Style.dismissButtonSize
                Layout.preferredHeight: Style.dismissButtonSize
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight

                visible: root.showDismissButton && !fileDetailsButton.visible

                icon.source: "image://svgimage-custom-color/clear.svg" + "/" + Style.ncTextColor

                flat: true
                display: Button.IconOnly
                hoverEnabled: true
                padding: 0

                NCToolTip {
                    text: qsTr("Dismiss")
                    visible: parent.hovered
                }

                onClicked: root.dismissButtonClicked()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Style.minimumActivityItemHeight
            Layout.maximumWidth: root.width - thumbnailItem.width
            spacing: Style.trayHorizontalMargin

            EnforcedPlainTextLabel {
                id: activityTextInfo

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                text: (root.activityData.type === "Sync") ? root.activityData.displayPath
                                                          : (root.activityData.type === "File") ? root.activityData.subject
                                                                                                : (root.activityData.type === "Notification") ? root.activityData.message
                                                                                                                                              : ""
                height: (text === "") ? 0 : implicitHeight
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.subLinePixelSize
                color: Style.ncTextColor
                visible: text !== ""
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: fileDetailsButton

                Layout.preferredWidth: Style.headerButtonIconSize
                Layout.preferredHeight: Style.headerButtonIconSize
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                icon.source: "image://svgimage-custom-color/more.svg"

                NCToolTip {
                    text: qsTr("Open file details")
                    visible: parent.hovered
                }

                flat: true
                display: Button.IconOnly
                hoverEnabled: true
                padding: 0

                visible: model.showFileDetails

                onClicked: Systray.presentShareViewInTray(model.openablePath)
            }

            EnforcedPlainTextLabel {
                id: talkReplyMessageSent

                height: (text === "") ? 0 : implicitHeight
                width: parent.width
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                text: root.activityData.messageSent
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font.pixelSize: Style.topLinePixelSize
                color: Style.ncSecondaryTextColor
                visible: text !== ""
            }

            ActivityItemActions {
                id: activityActions

                visible: !isFileActivityList && activityData.linksForActionButtons.length > 0 && !isTalkReplyOptionVisible

                Layout.fillWidth: true
                Layout.leftMargin: Style.trayListItemIconSize + Style.trayHorizontalMargin
                Layout.preferredHeight: Style.standardPrimaryButtonHeight
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                displayActions: activityData.displayActions
                objectType: activityData.objectType
                linksForActionButtons: activityData.linksForActionButtons
                linksContextMenu: activityData.linksContextMenu

                maxActionButtons: activityModel.maxActionButtons

                onTriggerAction: activityModel.slotTriggerAction(model.activityIndex, actionIndex)

                onShowReplyField: isTalkReplyOptionVisible = true
            }
        }
    }
}
