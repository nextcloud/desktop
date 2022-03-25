import QtQuick 2.15
import Style 1.0
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import com.nextcloud.desktopclient 1.0

Item {
    id: root

    Connections {
        target: activityModel
        function onMessageSent() {
            replyMessageTextField.clear();
            replyMessageSent.text = activityModel.replyMessageSent(model.index);
        }
    }

    function sendReplyMessage() {
        if (replyMessageTextField.text === "") {
            return;
        }

        UserModel.currentUser.sendReplyMessage(model.index, model.conversationToken, replyMessageTextField.text, model.messageId);
    }

    Text {
        id: replyMessageSent
        font.pixelSize: Style.topLinePixelSize
        color: Style.menuBorder
        visible: replyMessageSent.text !== ""
    }

    TextField {
        id: replyMessageTextField

        // TODO use Layout to manage width/height. The Layout.minimunWidth does not apply to the width set.
        height: 38
        width: 250

        onAccepted: root.sendReplyMessage()
        visible: replyMessageSent.text === ""

        topPadding: 4

        placeholderText: qsTr("Reply to …")

        background: Rectangle {
            id: replyMessageTextFieldBorder
            radius: 24
            border.width: 1
            border.color: Style.ncBlue
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

            icon {
                source: "image://svgimage-custom-color/send.svg" + "/" + Style.ncBlue
                width: 38
                height: 38
                color: hovered || !sendReplyMessageButton.enabled? Style.menuBorder : Style.ncBlue
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
