import QtQuick
import QtQuick.Controls
import com.strato.hidrivenext.desktopclient 

import Style

Button{
  id: root
  property string iconSource
  property int textWrapMode: Text.WordWrap

  property bool isMouseOver: false

  hoverEnabled: false // turn off default button hover

  contentItem: Row {
    id: contentRow
    spacing: Style.sesPillButtonVerticalPadding
    padding: Style.sesPillButtonVerticalPadding
    leftPadding: Style.sesPillButtonHorizontalPadding
    rightPadding: Style.sesPillButtonHorizontalPadding
    anchors.centerIn: parent
    Text {
        id: pillText
        text: root.text
        color: "white"
        font.weight: Style.sesFontNormalWeight
        font.pixelSize: Style.sesFontHintPixelSize
        maximumLineCount: 2
        elide: Text.ElideRight
        wrapMode: root.textWrapMode
        width: Math.min(implicitWidth, root.width - contentRow.leftPadding - contentRow.rightPadding - (root.iconSource ? Style.sesPillIconSize + contentRow.spacing : 0))
        anchors.verticalCenter: parent.verticalCenter
    }
    Image {
      visible: root.iconSource
      source: root.iconSource
      width: Style.sesPillIconSize
      height: Style.sesPillIconSize
      anchors.verticalCenter: parent.verticalCenter
    }
  }

  background: Rectangle {
    color: Style.sesPillButtonPrimaryBackgroundColor
    opacity: root.isMouseOver ? Style.sesPillButtonHoverOpacity : 1.0
    border.width: 2
    border.color: Style.sesPillButtonBorderColor
    radius: height / 2

    Behavior on opacity {
        NumberAnimation { duration: Style.shortAnimationDuration }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onExited: root.isMouseOver = false
        onEntered: root.isMouseOver = true
    }
  }
}