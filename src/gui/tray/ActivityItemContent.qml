import QtQml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Style
import Qt5Compat.GraphicalEffects
import com.ionos.hidrivenext.desktopclient

RowLayout {
    id: root

    property variant activityData: {{}}

    property variant activity: {{}}

    property bool showDismissButton: false

    property bool childHovered: fileDetailsButton.hovered || dismissActionButton.hovered

    property int iconSize: Style.trayListItemIconSize

    signal dismissButtonClicked()

    spacing: Style.standardSpacing

    Item {
        id: thumbnailItem

        readonly property int imageWidth: Style.sesIconSize
        readonly property int imageHeight: Style.sesIconSize
        readonly property int thumbnailRadius: model.thumbnail && model.thumbnail.isUserAvatar ? width / 2 : 3

        implicitWidth: Style.sesIconSize
        implicitHeight: Style.sesIconSize

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
                    width: Style.sesIconSize
                    height: Style.sesIconSize
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

            width: model.thumbnail !== undefined ? Style.sesIconSize * 0.6 : Style.sesIconSize
            height: model.thumbnail !== undefined ? Style.sesIconSize * 0.6 : Style.sesIconSize

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
            source: model.icon + "/" + palette.text
            sourceSize.height: 64
            sourceSize.width: 64
            mipmap: true // Addresses grainy downscale
        }
    }

    ColumnLayout {
        id: activityContentLayout

        Layout.fillHeight: true
        Layout.fillWidth: true
        Layout.maximumWidth: root.width - Style.standardSpacing - root.iconSize + Style.sesActivityItemWidthModifier
        implicitWidth: root.width - Style.standardSpacing - root.iconSize + Style.sesActivityItemWidthModifier

        spacing: Style.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            EnforcedPlainTextLabel {
                id: activityTextTitle
                text: (root.activityData.type === "Activity" || root.activityData.type === "Notification") ? root.activityData.subject : root.activityData.message

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter | Qt.AlignLeft

                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font: root.font
                visible: text !== ""
            }

            Item {
                Layout.fillWidth: true
                Layout.leftMargin: -Style.trayHorizontalMargin
            }

            EnforcedPlainTextLabel {
                id: activityTextDateTime

                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                height: (text === "") ? 0 : implicitHeight

                text: root.activityData.dateTime
                font.family: Style.sesOpenSansRegular
                font.pixelSize: Style.sesFontHintPixelSize
                color: Style.sesTrayFontColor
                visible: text !== ""
            }

            Row {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
                spacing: Style.extraSmallSpacing

                IconButton {
                    id: fileDetailsButton   
                 
                    property bool isHovered: fileDetailsButton.hovered || fileDetailsButton.visualFocus
                    property bool isActive: fileDetailsButton.pressed
                    
                    Layout.preferredWidth: Style.dismissButtonSize
                    Layout.preferredHeight: Style.dismissButtonSize
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight

                    width: Style.activityListButtonWidth
                    height: Style.activityListButtonHeight
                    icon.source: "image://svgimage-custom-color/more.svg/" + (isHovered ? Style.sesWhite : Style.sesActionHover)
                    
                    icon.width: Style.activityListButtonIconSize
                    icon.height: Style.activityListButtonIconSize

                    ToolTip {
                        text: qsTr("Open file details")
                        visible: parent.hovered
                    }
                    background: Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        color: parent.isActive ? Style.sesActionPressed : parent.isHovered ? Style.sesActionHover : "transparent"
                        radius: width / 2
                    }

                    display: Button.IconOnly
                    leftPadding: 0
                    rightPadding: 0

                    visible: model.showFileDetails
                    onClicked: Systray.presentShareViewInTray(model.openablePath)
                }

                Button {
                    id: dismissActionButton

                    width: Style.activityListButtonWidth
                    height: Style.activityListButtonHeight

                    icon.source: "image://svgimage-custom-color/clear.svg/" + palette.buttonText
                    icon.width: Style.activityListButtonIconSize
                    icon.height: Style.activityListButtonIconSize

                    display: Button.IconOnly

                    ToolTip {
                        text: qsTr("Dismiss")
                        visible: parent.hovered
                    }

                    visible: root.showDismissButton
                    onClicked: root.dismissButtonClicked()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: activityTextInfo.visible || talkReplyMessageSent.visible || activityActions.visible

            EnforcedPlainTextLabel {
                id: activityTextInfo

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop | Qt.AlignLeft

                text: (root.activityData.type === "Sync") ? root.activityData.subject
                                                          : (root.activityData.type === "File") ? root.activityData.subject
                                                                                                : (root.activityData.type === "Notification") ? root.activityData.message
                                                                                                                                              : ""
                height: (text === "") ? 0 : implicitHeight
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 10
                font.family: Style.sesOpenSansRegular
                font.pixelSize: Style.sesFontHintPixelSize
                visible: text !== ""
            }

            Item {
                Layout.fillWidth: true
                visible: !talkReplyMessageSent.visible
            }

            EnforcedPlainTextLabel {
                id: talkReplyMessageSent

                height: (text === "") ? 0 : implicitHeight
                Layout.maximumWidth: parent.width / 2
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                text: root.activityData.messageSent
                elide: Text.ElideRight
                wrapMode: Text.Wrap
                maximumLineCount: 2
                font: root.font
                color: Style.sesTrayFontColor                
                visible: text !== ""
            }

            ActivityItemActions {
                id: activityActions

                visible: !isFileActivityList && activityData.linksForActionButtons.length > 0

                Layout.fillWidth: true
                Layout.leftMargin: Style.trayListItemIconSize + Style.trayHorizontalMargin
                Layout.preferredHeight: Style.standardPrimaryButtonHeight
                Layout.alignment: Qt.AlignTop | Qt.AlignRight

                displayActions: activityData.displayActions
                objectType: activityData.objectType
                linksForActionButtons: activityData.linksForActionButtons
                linksContextMenu: activityData.linksContextMenu

                maxActionButtons: activityModel.maxActionButtons

                onTriggerAction: activityModel.slotTriggerAction(activityData.activityIndex, actionIndex)

                onShowReplyField: isTalkReplyOptionVisible = true
                talkReplyButtonVisible: root.activityData.messageSent === "" && !isTalkReplyOptionVisible
            }
        }
    }
}
