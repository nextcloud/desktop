import QtQuick 2.15
import Style 1.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.nextcloud.desktopclient 1.0

Item {
    id: root

    signal sendReply(string reply)

    function sendReplyMessage() {
        if (replyMessageTextField.text === "") {
            return;
        }

        root.sendReply(replyMessageTextField.text);
    }

    height: 38
    width: 250

    TextField {
        id: replyMessageTextField

        anchors.fill: parent
        topPadding: 4
        rightPadding: sendReplyMessageButton.width
        visible: model.messageSent === ""

        color: Style.ncSecondaryTextColor
        placeholderText: qsTr("Reply to …")

        onAccepted: root.sendReplyMessage()

        background: Rectangle {
            id: replyMessageTextFieldBorder
            radius: 24
            border.width: 1
            border.color: parent.activeFocus ? UserModel.currentUser.accentColor : Style.menuBorder
            color: Style.backgroundColor
        }

        Button {
            id: sendReplyMessageButton  
            width: 32
            height: parent.height
            opacity: 0.8
            flat: true
            enabled: replyMessageTextField.text !== ""
            onClicked: root.sendReplyMessage()
            background: Rectangle {
                color: "transparent"
            }

            icon {
                source: "image://svgimage-custom-color/send.svg" + "/" + Style.menuBorder
                width: 38
                height: 38
                color: hovered || !sendReplyMessageButton.enabled? Style.menuBorder : UserModel.currentUser.accentColor
            }

            anchors {
                right: replyMessageTextField.right
                top: replyMessageTextField.top
            }

            ToolTip {
                visible: sendReplyMessageButton.hovered
                delay: Qt.styleHints.mousePressAndHoldInterval
                text:  qsTr("Send reply to chat message")
            }
        }
    }
}
