import QtQuick 2.15
import QtQuick.Window 2.15
import Style 1.0
import com.nextcloud.desktopclient 1.0
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

ApplicationWindow {
    id: root
    flags: Qt.Dialog | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    color: "transparent"

    width: 320
    height: contentLayout.implicitHeight
    modality: Qt.ApplicationModal

    readonly property real fontPixelSize: Style.topLinePixelSize * 1.5
    readonly property real iconWidth: fontPixelSize * 2

    // TODO: Rather than setting all these palette colours manually,
    // create a custom style and do it for all components globally
    palette {
        text: Style.ncTextColor
        windowText: Style.ncTextColor
        buttonText: Style.ncTextColor
        brightText: Style.ncTextBrightColor
        highlight: Style.lightHover
        highlightedText: Style.ncTextColor
        light: Style.lightHover
        midlight: Style.ncSecondaryTextColor
        mid: Style.darkerHover
        dark: Style.menuBorder
        button: Style.buttonBackgroundColor
        window: Style.backgroundColor
        base: Style.backgroundColor
        toolTipBase: Style.backgroundColor
        toolTipText: Style.ncTextColor
    }

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
        color: Style.backgroundColor
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
            id: labelMessage
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            Layout.bottomMargin: Style.standardSpacing
            text: qsTr("Discovering the certificates stored on your USB token")
            elide: Text.ElideRight
            font.pixelSize: root.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
