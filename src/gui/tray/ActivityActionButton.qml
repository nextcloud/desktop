import QtQuick
import QtQuick.Controls
import Style
import com.nextcloud.desktopclient

AbstractButton {
    id: root

    property string toolTipText: ""

    property bool primaryButton: false

    readonly property color adjustedHeaderColor: Style.adjustedCurrentUserHeaderColor

    property string verb: ""
    property bool isTalkReplyButton: false

    leftPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing
    rightPadding: root.text === "" ? Style.smallSpacing : Style.standardSpacing

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
            bold: root.primaryButton
        }
    }

    Component {
        id: primaryButtonContent
        NCButtonContents {
            anchors.fill: parent
            hovered: root.hovered
            imageSource: root.icon.source
            text: root.text
        }
    }
}
