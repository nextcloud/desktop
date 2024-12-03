import QtQuick
import QtQuick.Window
import Style
import com.nextcloud.desktopclient
import QtQuick.Layouts
import QtQuick.Controls

ApplicationWindow {
    id: root
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    color: palette.base

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    width: 320
    height: contentLayout.implicitHeight

    property string fileName: ""

    readonly property real fontPixelSize: Style.topLinePixelSize * 1.5
    readonly property real iconWidth: fontPixelSize * 2

    Component.onCompleted: {
        Systray.forceWindowInit(root);
        x = Screen.width / 2 - width / 2
        y = Screen.height / 2 - height / 2
        root.show();
        root.raise();
        root.requestActivate();
    }

    Rectangle {
        id: windowBackground
        color: palette.base
        radius: Style.trayWindowRadius
        border.color: palette.dark
        anchors.fill: parent
    }

    ColumnLayout {
        id: contentLayout
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Style.standardSpacing
        anchors.rightMargin: Style.standardSpacing
        spacing: Style.standardSpacing

        NCBusyIndicator {
            id: busyIndicator
            Layout.topMargin: Style.standardSpacing
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.iconWidth
            Layout.preferredHeight: root.iconWidth
            imageSourceSizeHeight: root.iconWidth
            imageSourceSizeWidth: root.iconWidth
            padding: 0
            color: palette.windowText
            running: true
        }
        EnforcedPlainTextLabel {
            id: labelFileName
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            text: root.fileName
            elide: Text.ElideMiddle
            font.bold: true
            font.pixelSize: root.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
            visible: root.fileName !== ""
        }
        EnforcedPlainTextLabel {
            id: labelMessage
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.bottomMargin: Style.standardSpacing
            text: qsTr("Opening file for local editing")
            elide: Text.ElideRight
            font.pixelSize: root.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
