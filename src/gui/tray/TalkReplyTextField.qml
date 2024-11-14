import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import com.nextcloud.desktopclient
import Style

TextField {
    id: replyMessageTextField

    signal sendReply(string reply)
    function sendReplyMessage() { if (text !== "") sendReply(text) }

    height: Style.talkReplyTextFieldPreferredHeight
    visible: model.messageSent === ""
    placeholderText: qsTr("Reply to …")

    onAccepted: sendReplyMessage()

    verticalAlignment: Qt.AlignVCenter
    background: Rectangle {
        border.color: palette.dark
        radius: Style.trayWindowRadius
        color: palette.base
    }

    Button {
        id: sendReplyMessageButton

        width: Style.talkReplyTextFieldPreferredWidth * 0.12
        height: parent.height

        opacity: 0.8
        flat: true
        enabled: replyMessageTextField.text !== ""
        onClicked: replyMessageTextField.sendReplyMessage()
        background: null

        icon {
            source: "image://svgimage-custom-color/send.svg" + "/" + palette.dark
            color: hovered || !sendReplyMessageButton.enabled ? palette.dark : UserModel.currentUser.accentColor
        }

        anchors {
            right: replyMessageTextField.right
            top: replyMessageTextField.top
        }

        ToolTip {
            visible: sendReplyMessageButton.hovered
            text:  qsTr("Send reply to chat message")
        }
    }
}

