import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

RowLayout {
    id: root

    property variant activityData: {{}}

    property color activityTextTitleColor: Style.ncTextColor

    property bool showDismissButton: false

    property bool childHovered: shareButton.hovered || dismissActionButton.hovered

    signal dismissButtonClicked()
    signal shareButtonClicked()

    spacing: 10

    Image {
        id: activityIcon

        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
        Layout.preferredWidth: 32
        Layout.preferredHeight: 32

        verticalAlignment: Qt.AlignCenter
        source: icon
        sourceSize.height: 64
        sourceSize.width: 64
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
            width: parent.width
            elide: Text.ElideRight
            wrapMode: Text.Wrap
            maximumLineCount: 2
            font.pixelSize: Style.topLinePixelSize
            color: root.activityData.activityTextTitleColor
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
            color: "#808080"
        }
    }

    Button {
        id: dismissActionButton

        Layout.preferredWidth: parent.height * 0.40
        Layout.preferredHeight: parent.height * 0.40

        Layout.alignment: Qt.AlignCenter

        Layout.margins: Style.roundButtonBackgroundVerticalMargins

        ToolTip.visible: hovered
        ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
        ToolTip.text: qsTr("Dismiss")

        Accessible.name: qsTr("Dismiss")

        visible: root.showDismissButton && !shareButton.visible

        background: Rectangle {
            color: "transparent"
        }

        contentItem: Image {
            anchors.fill: parent
            source: parent.hovered ? "image://svgimage-custom-color/clear.svg/black" : "image://svgimage-custom-color/clear.svg/grey"
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
        imageSourceHover: "image://svgimage-custom-color/share.svg" + "/" + Style.ncTextColor

        toolTipText: qsTr("Open share dialog")

        bgColor: UserModel.currentUser.headerColor

        onClicked: root.shareButtonClicked()
    }
}
