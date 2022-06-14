import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import QtGraphicalEffects 1.15
import com.nextcloud.desktopclient 1.0

RowLayout {
    id: root

    property variant activityData: {{}}

    property color activityTextTitleColor: Style.ncTextColor

    property bool showDismissButton: false

    property bool childHovered: shareButton.hovered || dismissActionButton.hovered

    property color adjustedHeaderColor: "transparent"

    signal dismissButtonClicked()
    signal shareButtonClicked()

    spacing: Style.trayHorizontalMargin

    Item {
        id: thumbnailItem
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.preferredWidth: Style.trayListItemIconSize
        Layout.preferredHeight: model.thumbnail && model.thumbnail.isMimeTypeIcon ? Style.trayListItemIconSize * 0.9 : Style.trayListItemIconSize
        readonly property int imageWidth: width * (1 - Style.thumbnailImageSizeReduction)
        readonly property int imageHeight: height * (1 - Style.thumbnailImageSizeReduction)
        readonly property int thumbnailRadius: model.thumbnail && model.thumbnail.isUserAvatar ? width / 2 : 3

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

            readonly property int negativeLeftMargin: -((width / 2) +
                                                        ((width - paintedWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailItem.imageWidth) / 2) +
                                                        ((thumbnailImageLoader.width - thumbnailImageLoader.item.paintedWidth) / 2) +
                                                        (thumbnailItem.thumbnailRadius / 4))
            readonly property int negativeTopMargin: -((height / 2) +
                                                       ((height - paintedHeight) / 2) +
                                                       ((thumbnailImageLoader.height - thumbnailItem.imageHeight) / 4) +
                                                       ((thumbnailImageLoader.height - thumbnailImageLoader.item.paintedHeight) / 4) +
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
        }
    }

    Column {
        id: activityTextColumn

        Layout.topMargin: Style.activityContentSpace
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

        spacing: Style.activityContentSpace

        Label {
            id: activityTextTitle
            text: (root.activityData.type === "Activity" || root.activityData.type === "Notification") ? root.activityData.subject : root.activityData.message
            height: (text === "") ? 0 : implicitHeight
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            font.pixelSize: Style.topLinePixelSize
            color: Style.ncTextColor
            visible: text !== ""
        }

        Label {
            id: activityTextInfo
            text: (root.activityData.type === "Sync") ? root.activityData.displayPath
                                    : (root.activityData.type === "File") ? root.activityData.subject
                                                        : (root.activityData.type === "Notification") ? root.activityData.message
                                                                                    : ""
            height: (text === "") ? 0 : implicitHeight
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            font.pixelSize: Style.subLinePixelSize
            color: Style.ncTextColor
            visible: text !== ""
        }

        Label {
            id: activityTextDateTime
            text: root.activityData.dateTime
            height: (text === "") ? 0 : implicitHeight
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            font.pixelSize: Style.subLinePixelSize
            color: Style.ncSecondaryTextColor
            visible: text !== ""
        }

        Label {
            id: talkReplyMessageSent
            text: root.activityData.messageSent
            height: (text === "") ? 0 : implicitHeight
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            font.pixelSize: Style.topLinePixelSize
            color: Style.ncSecondaryTextColor
            visible: text !== ""
        }
    }

    Button {
        id: dismissActionButton

        Layout.preferredWidth: Style.trayListItemIconSize * 0.6
        Layout.preferredHeight: Style.trayListItemIconSize * 0.6

        Layout.alignment: Qt.AlignCenter

        Layout.margins: Style.roundButtonBackgroundVerticalMargins

        ToolTip {
            id: dismissActionButtonTooltip
            visible: parent.hovered
            delay: Qt.styleHints.mousePressAndHoldInterval
            text: qsTr("Dismiss")
            contentItem: Label {
                text: dismissActionButtonTooltip.text
                color: Style.ncTextColor
            }
            background: Rectangle {
                border.color: Style.menuBorder
                color: Style.backgroundColor
            }
        }

        Accessible.name: qsTr("Dismiss")

        visible: root.showDismissButton && !shareButton.visible

        background: Rectangle {
            color: "transparent"
        }

        contentItem: Image {
            anchors.fill: parent
            source: parent.hovered ? Theme.darkMode ?
                "image://svgimage-custom-color/clear.svg/white" : "image://svgimage-custom-color/clear.svg/black" :
                "image://svgimage-custom-color/clear.svg/grey"
            sourceSize.width: 24
            sourceSize.height: 24
        }

        onClicked: root.dismissButtonClicked()
    }

    CustomButton {
        id: shareButton

        Layout.preferredWidth: Style.trayListItemIconSize
        Layout.preferredHeight: Style.trayListItemIconSize

        visible: root.activityData.isShareable

        imageSource: "image://svgimage-custom-color/share.svg" + "/" + root.adjustedHeaderColor
        imageSourceHover: "image://svgimage-custom-color/share.svg" + "/" + UserModel.currentUser.headerTextColor

        toolTipText: qsTr("Open share dialog")

        bgColor: UserModel.currentUser.headerColor

        onClicked: root.shareButtonClicked()
    }
}
