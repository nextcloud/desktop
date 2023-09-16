import QtQuick 2.15
import QtQuick.Controls 2.3
import Style 1.0
import com.nextcloud.desktopclient 1.0

AbstractButton {
    id: root

    property string toolTipText: ""

    property bool primaryButton: false

    property string imageSourceHover: ""

    readonly property color adjustedHeaderColor: Style.adjustedCurrentUserHeaderColor
    readonly property color textColor: primaryButton ? adjustedHeaderColor : palette.buttonText
    readonly property color textColorHovered: primaryButton ? Style.currentUserHeaderTextColor : palette.buttonText

    property string verb: ""
    property bool isTalkReplyButton: false

    leftPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing
    rightPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing

    background: NCButtonBackground {
        color: Style.currentUserHeaderColor
        hovered: root.hovered
        visible: root.primaryButton
    }

    contentItem: Loader {
        id: contentItemLoader
        active: true
        sourceComponent: root.primaryButton ? primaryButtonContent : textButtonContent
    }

    ToolTip {
        id: customTextButtonTooltip
        text: root.toolTipText
        delay: Qt.styleHints.mousePressAndHoldInterval
        visible: root.toolTipText !== "" && root.hovered
        contentItem: EnforcedPlainTextLabel { text: customTextButtonTooltip.text }
    }

    Component {
        id: textButtonContent
        TextButtonContents {
            anchors.fill: parent
            hovered: root.hovered
            text: root.text
            textColor: root.textColor
            textColorHovered: root.textColorHovered

            bold: root.primaryButton
        }
    }

    Component {
        id: primaryButtonContent
        NCButtonContents {
            anchors.fill: parent
            hovered: root.hovered
            imageSourceHover: root.imageSourceHover
            imageSource: root.icon.source
            text: root.text
            textColor: root.textColor
            textColorHovered: root.textColorHovered
            font.bold: root.primaryButton
        }
    }
}
