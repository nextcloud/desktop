import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

ItemDelegate {
    id: root

    property Flickable flickable

    property int iconSize: Style.trayListItemIconSize

    property bool isFileActivityList: false

    readonly property bool isChatActivity: model.objectType === "chat" || model.objectType === "room" || model.objectType === "call"
    readonly property bool isTalkReplyPossible: model.conversationToken !== ""
    property bool isTalkReplyOptionVisible: model.messageSent !== ""

    padding: Style.standardSpacing

    Accessible.role: Accessible.ListItem
    Accessible.name: (model.path !== "" && model.displayPath !== "") ? qsTr("Open %1 locally").arg(model.displayPath) : model.message
    Accessible.onPressAction: root.clicked()

    NCToolTip {
        visible: root.hovered && !activityContent.childHovered && model.displayLocation !== ""
        text: qsTr("In %1").arg(model.displayLocation)
    }

    // TODO - Fix warning: QML ColumnLayout: The current style does not support customization of this control
    // (property: "contentItem" item: QQuickColumnLayout(0x6000014cac10, parent=0x0, geometry=0,0 0x0)). P
    // lease customize a non-native style (such as Basic, Fusion, Material, etc).
    // For more information, see: https://doc.qt.io/qt-6/qtquickcontrols2-customize.html#customization-reference
    contentItem: ColumnLayout {
        spacing: Style.smallSpacing

        ActivityItemContent {
            id: activityContent

            Layout.fillWidth: true
            Layout.minimumHeight: Style.minActivityHeight
            Layout.preferredWidth: parent.width

            showDismissButton: model.isDismissable

            iconSize: root.iconSize

            activityData: model
            activity: model.activity

            onDismissButtonClicked: activityModel.slotTriggerDismiss(model.activityIndex)
        }

        Loader {
            id: talkReplyTextFieldLoader
            active: root.isChatActivity && root.isTalkReplyPossible && model.messageSent === ""
            visible: root.isTalkReplyOptionVisible

            Layout.preferredWidth: Style.talkReplyTextFieldPreferredWidth
            Layout.preferredHeight: Style.talkReplyTextFieldPreferredHeight
            Layout.leftMargin: Style.trayListItemIconSize + Style.trayHorizontalMargin

            sourceComponent: TalkReplyTextField {
                onSendReply: {
                    UserModel.currentUser.sendReplyMessage(model.activityIndex, model.conversationToken, reply, model.messageId);
                    talkReplyTextFieldLoader.visible = false;
                }
            }
        }
    }
}
