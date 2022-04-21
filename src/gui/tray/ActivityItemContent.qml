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

    signal dismissButtonClicked()
    signal shareButtonClicked()

    spacing: Style.trayHorizontalMargin

    Item {
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.preferredWidth: Style.trayListItemIconSize
        Layout.preferredHeight: Style.trayListItemIconSize

        Loader {
            id: thumbnailImageLoader
            anchors.fill: parent
            active: model.thumbnail !== undefined

            sourceComponent: Item {
                anchors.fill: parent

                Image {
                    id: thumbnailImage
                    width: model.thumbnail.isMimeTypeIcon ? parent.width * 0.85 : parent.width * 0.8
                    height: model.thumbnail.isMimeTypeIcon ? parent.height * 0.85 : parent.height * 0.8
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    cache: true
                    source: model.thumbnail.source
                    visible: false
                    sourceSize.height: 64
                    sourceSize.width: 64
                }

                Rectangle {
                    id: mask
                    color: "white"
                    radius: 3
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
            width: model.thumbnail !== undefined ? parent.width * 0.5 : parent.width * 0.85
            height: model.thumbnail !== undefined ? parent.height * 0.5 : parent.height * 0.85
            anchors.verticalCenter: if(model.thumbnail === undefined) parent.verticalCenter
            anchors.left: if(model.thumbnail === undefined) parent.left
            anchors.right: if(model.thumbnail !== undefined) parent.right
            anchors.bottom: if(model.thumbnail !== undefined) parent.bottom
            cache: true

            source: Theme.darkMode ? model.darkIcon : model.lightIcon
            sourceSize.height: 64
            sourceSize.width: 64
        }
    }

    Column {
        id: activityTextColumn

        Layout.topMargin: 4
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

        spacing: 4

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

        Loader {
            id: talkReplyTextFieldLoader
            active: isChatActivity && isTalkReplyPossible
            visible: isTalkReplyOptionVisible

            anchors.top: activityTextDateTime.bottom
            anchors.topMargin: 10

            sourceComponent: TalkReplyTextField {
                id: talkReplyMessage
                anchors.fill: parent
            }
        }
    }

    Button {
        id: dismissActionButton

        Layout.preferredWidth: parent.height * 0.40
        Layout.preferredHeight: parent.height * 0.40

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

        Layout.preferredWidth: parent.height * 0.70
        Layout.preferredHeight: parent.height * 0.70

        visible: root.activityData.isShareable

        imageSource: "image://svgimage-custom-color/share.svg" + "/" + UserModel.currentUser.headerColor
        imageSourceHover: "image://svgimage-custom-color/share.svg" + "/" + UserModel.currentUser.headerTextColor

        toolTipText: qsTr("Open share dialog")

        bgColor: UserModel.currentUser.headerColor

        onClicked: root.shareButtonClicked()
    }
}
