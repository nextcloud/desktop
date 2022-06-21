import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Style 1.0
import com.nextcloud.desktopclient 1.0

MouseArea {
    id: root

    property Flickable flickable

    property bool isFileActivityList: false

    readonly property bool isChatActivity: model.objectType === "chat" || model.objectType === "room" || model.objectType === "call"
    readonly property bool isTalkReplyPossible: model.conversationToken !== ""
    property bool isTalkReplyOptionVisible: model.messageSent !== ""
    readonly property bool isCallActivity: model.objectType === "call"

    property color adjustedHeaderColor: Theme.darkMode ? Qt.lighter(UserModel.currentUser.headerColor, 2)
                                                       : Qt.darker(UserModel.currentUser.headerColor, 1.5)

    signal fileActivityButtonClicked(string absolutePath)

    enabled: (model.path !== "" || model.link !== "" || model.isCurrentUserFileActivity === true)
    hoverEnabled: true

    // We center the children vertically in the middle of this MouseArea to create the padding.
    height: contentLayout.implicitHeight + (Style.standardSpacing * 2)

    Accessible.role: Accessible.ListItem
    Accessible.name: (model.path !== "" && model.displayPath !== "") ? qsTr("Open %1 locally").arg(model.displayPath) : model.message
    Accessible.onPressAction: root.clicked()

    function toggleReplyOptions() {
        isTalkReplyOptionVisible = !isTalkReplyOptionVisible
    }

    Rectangle {
        id: activityHover
        anchors.fill: parent
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }

    ToolTip {
        id: activityMouseAreaTooltip
        visible: containsMouse && !activityContent.childHovered && model.displayLocation !== ""
        delay: Qt.styleHints.mousePressAndHoldInterval
        text: qsTr("In %1").arg(model.displayLocation)
        contentItem: Label {
            text: activityMouseAreaTooltip.text
            color: Style.ncTextColor
        }
        background: Rectangle {
            border.color: Style.menuBorder
            color: Style.backgroundColor
        }
    }

    ColumnLayout {
        id: contentLayout
        anchors.left: root.left
        anchors.right: root.right
        anchors.rightMargin: Style.standardSpacing
        anchors.leftMargin: Style.standardSpacing
        anchors.verticalCenter: parent.verticalCenter

        spacing: Style.activityContentSpace

        ActivityItemContent {
            id: activityContent

            Layout.fillWidth: true
            Layout.minimumHeight: Style.minActivityHeight

            showDismissButton: model.links.length > 0 && model.linksForActionButtons.length === 0

            activityData: model

            adjustedHeaderColor: root.adjustedHeaderColor

            onShareButtonClicked: Systray.openShareDialog(model.displayPath, model.absolutePath)
            onDismissButtonClicked: activityModel.slotTriggerDismiss(model.index)
        }

        Loader {
            id: talkReplyTextFieldLoader
            active: root.isChatActivity && root.isTalkReplyPossible && model.messageSent === ""
            visible: root.isTalkReplyOptionVisible

            Layout.leftMargin: Style.trayListItemIconSize + activityContent.spacing
            Layout.preferredHeight: root.isTalkReplyOptionVisible ? implicitHeight : 0

            sourceComponent: TalkReplyTextField {
                onSendReply: {
                    UserModel.currentUser.sendReplyMessage(model.index, model.conversationToken, reply, model.messageId);
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

            moreActionsButtonColor: activityHover.color
            maxActionButtons: activityModel.maxActionButtons

            flickable: root.flickable

            adjustedHeaderColor: root.adjustedHeaderColor

            onTriggerAction: activityModel.slotTriggerAction(model.index, actionIndex)
            onShowReplyField: root.toggleReplyOptions()
        }
    }
}
