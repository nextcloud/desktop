import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

ItemDelegate {
    id: root

    property Flickable flickable

    property bool isFileActivityList: false

    readonly property bool isChatActivity: model.objectType === "chat" || model.objectType === "room" || model.objectType === "call"
    readonly property bool isTalkReplyPossible: model.conversationToken !== ""
    property bool isTalkReplyOptionVisible: model.messageSent !== ""

    property color adjustedHeaderColor: Theme.darkMode ? Qt.lighter(UserModel.currentUser.headerColor, 2)
                                                       : Qt.darker(UserModel.currentUser.headerColor, 1.5)

    enabled: (model.path !== "" || model.link !== "" || model.links.length > 0 ||  model.isCurrentUserFileActivity === true)
    padding: Style.standardSpacing

    Accessible.role: Accessible.ListItem
    Accessible.name: (model.path !== "" && model.displayPath !== "") ? qsTr("Open %1 locally").arg(model.displayPath) : model.message
    Accessible.onPressAction: root.clicked()

    NCToolTip {
        visible: root.hovered && !activityContent.childHovered && model.displayLocation !== ""
        text: qsTr("In %1").arg(model.displayLocation)
    }

    contentItem: ColumnLayout {
        id: contentLayout
        anchors.left: root.left
        anchors.right: root.right
        anchors.rightMargin: Style.standardSpacing
        anchors.leftMargin: Style.standardSpacing

        spacing: Style.activityContentSpace

        ActivityItemContent {
            id: activityContent

            Layout.fillWidth: true
            Layout.minimumHeight: Style.minActivityHeight

            showDismissButton: model.links.length > 0

            activityData: model

            adjustedHeaderColor: root.adjustedHeaderColor

            onShareButtonClicked: Systray.openShareDialog(model.displayPath, model.path)

            onDismissButtonClicked: activityModel.slotTriggerDismiss(model.activityIndex)
        }

        Loader {
            id: talkReplyTextFieldLoader
            active: root.isChatActivity && root.isTalkReplyPossible && model.messageSent === ""
            visible: root.isTalkReplyOptionVisible

            Layout.preferredWidth: Style.talkReplyTextFieldPreferredWidth
            Layout.preferredHeight: Style.talkReplyTextFieldPreferredHeight
            Layout.leftMargin: Style.trayListItemIconSize + activityContent.spacing

            sourceComponent: TalkReplyTextField {
                onSendReply: {
                    UserModel.currentUser.sendReplyMessage(model.activityIndex, model.conversationToken, reply, model.messageId);
                    talkReplyTextFieldLoader.visible = false;
                }
            }
        }

        ActivityItemActions {
            id: activityActions

            visible: !root.isFileActivityList && model.linksForActionButtons.length > 0 && !isTalkReplyOptionVisible

            Layout.fillWidth: true
            Layout.leftMargin: Style.trayListItemIconSize + activityContent.spacing
            Layout.minimumHeight: Style.minActivityHeight

            displayActions: model.displayActions
            objectType: model.objectType
            linksForActionButtons: model.linksForActionButtons
            linksContextMenu: model.linksContextMenu

            maxActionButtons: activityModel.maxActionButtons

            flickable: root.flickable

            adjustedHeaderColor: root.adjustedHeaderColor

            onTriggerAction: activityModel.slotTriggerAction(model.activityIndex, actionIndex)
            onShowReplyField: root.isTalkReplyOptionVisible = true
        }
    }
}
