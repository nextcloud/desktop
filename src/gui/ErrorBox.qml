import QtQuick 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

import Style 1.0

Item {
    id: errorBox

    signal closeButtonClicked
    
    property string text: ""

    property color backgroundColor: Style.errorBoxBackgroundColor
    property bool showCloseButton: false
    
    implicitHeight: errorMessageLayout.implicitHeight + (2 * Style.standardSpacing)

    Rectangle {
        id: solidStripe

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left

        width: Style.errorBoxStripeWidth
        color: errorBox.backgroundColor
    }

    Rectangle {
        anchors.fill: parent
        color: errorBox.backgroundColor
        opacity: 0.2
    }

    GridLayout {
        id: errorMessageLayout

        anchors.fill: parent
        anchors.margins: Style.standardSpacing
        anchors.leftMargin: Style.standardSpacing + solidStripe.width

        columns: 2

        Label {
            Layout.fillWidth: true
            color: Style.ncTextColor
            font.bold: true
            text: qsTr("Error")
            visible: errorBox.showCloseButton
        }

        Button {
            Layout.preferredWidth: Style.iconButtonWidth
            Layout.preferredHeight: Style.iconButtonWidth

            background: null
            icon.color: Style.ncTextColor
            icon.source: "qrc:///client/theme/close.svg"

            visible: errorBox.showCloseButton
            enabled: visible

            onClicked: errorBox.closeButtonClicked()
        }

        Label {
            id: errorMessage

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.columnSpan: 2

            color: Style.ncTextColor
            wrapMode: Text.WordWrap
            text: errorBox.text
            textFormat: Text.PlainText
        }
    }
}
