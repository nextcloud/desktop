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

    property bool isChatActivity: model.objectType === "chat" || model.objectType === "room"

    signal fileActivityButtonClicked(string absolutePath)

    enabled: (model.path !== "" || model.link !== "" || model.isCurrentUserFileActivity === true)
    hoverEnabled: true

    height: childrenRect.height

    ToolTip.visible: containsMouse && !activityContent.childHovered && model.displayLocation !== ""
    ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
    ToolTip.text: qsTr("In %1").arg(model.displayLocation)

    Accessible.role: Accessible.ListItem
    Accessible.name: (model.path !== "" && model.displayPath !== "") ? qsTr("Open %1 locally").arg(model.displayPath) : model.message
    Accessible.onPressAction: root.clicked()

    Rectangle {
        id: activityHover
        anchors.fill: parent
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }

    ColumnLayout {
        anchors.left: root.left
        anchors.right: root.right
        anchors.leftMargin: 15
        anchors.rightMargin: 10

        spacing: 0

        ActivityItemContent {
            id: activityContent

            Layout.fillWidth: true

            showDismissButton: model.links.length > 0 && model.linksForActionButtons.length === 0

            activityData: model

            Layout.preferredHeight: Style.trayWindowHeaderHeight

            onShareButtonClicked: Systray.openShareDialog(model.displayPath, model.absolutePath)
            onDismissButtonClicked: activityModel.slotTriggerDismiss(model.index)
        }

        ActivityItemActions {
            id: activityActions

            visible: !root.isFileActivityList && model.linksForActionButtons.length > 0

            Layout.preferredHeight: Style.trayWindowHeaderHeight * 0.85
            Layout.fillWidth: true
            Layout.leftMargin: 40
            Layout.bottomMargin: model.links.length > 1 ? 5 : 0

            displayActions: model.displayActions
            objectType: model.objectType
            linksForActionButtons: model.linksForActionButtons
            linksContextMenu: model.linksContextMenu

            moreActionsButtonColor: activityHover.color
            maxActionButtons: activityModel.maxActionButtons

            flickable: root.flickable

            onTriggerAction: activityModel.slotTriggerAction(model.index, actionIndex)
        }
    }
}
