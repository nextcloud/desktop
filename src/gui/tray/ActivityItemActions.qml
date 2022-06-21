import QtQml 2.15
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

RowLayout {
    id: root

    spacing: 20

    property string objectType: ""
    property variant linksForActionButtons: []
    property variant linksContextMenu: []
    property bool displayActions: false

    property color moreActionsButtonColor: "transparent"
    property color adjustedHeaderColor: "transparent"

    property int maxActionButtons: 0

    property Flickable flickable

    signal triggerAction(int actionIndex)
    signal showReplyField()

    Repeater {
        id: actionsRepeater
        // a max of maxActionButtons will get dispayed as separate buttons
        model: root.linksForActionButtons

        ActivityActionButton {
            id: activityActionButton

            readonly property string verb: model.modelData.verb
            readonly property bool primary: model.index === 0 && verb !== "DELETE"
            readonly property bool isTalkReplyButton: verb === "REPLY"

            Layout.minimumWidth: primary ? Style.activityItemActionPrimaryButtonMinWidth : Style.activityItemActionSecondaryButtonMinWidth
            Layout.preferredHeight: primary ? parent.height : parent.height * 0.3
            Layout.preferredWidth: primary ? -1 : parent.height

            text: model.modelData.label
            toolTipText: model.modelData.label

            imageSource: model.modelData.imageSource ? model.modelData.imageSource + root.adjustedHeaderColor : ""
            imageSourceHover: model.modelData.imageSourceHovered ? model.modelData.imageSourceHovered + UserModel.currentUser.headerTextColor : ""

            textColor: imageSource !== "" ? root.adjustedHeaderColor : Style.ncTextColor
            textColorHovered: imageSource !== "" ? UserModel.currentUser.headerTextColor : Style.ncTextColor

            bold: primary

            onClicked: !isTalkReplyButton ? root.triggerAction(model.index) : root.showReplyField()
        }
    }

    Loader {
        // actions that do not fit maxActionButtons limit, must be put into a context menu
        id: moreActionsButtonContainer

        Layout.preferredWidth: parent.height
        Layout.topMargin: Style.roundedButtonBackgroundVerticalMargins
        Layout.bottomMargin: Style.roundedButtonBackgroundVerticalMargins
        Layout.fillHeight: true

        active: root.displayActions && (root.linksContextMenu.length > 0)

        sourceComponent: Button {
            id: moreActionsButton

            icon.source: "qrc:///client/theme/more.svg"
            icon.color: Style.ncTextColor

            background: Rectangle {
                color: parent.hovered ? Style.lightHover : root.moreActionsButtonColor
                radius: width / 2
            }

            ToolTip {
                id: moreActionsButtonTooltip
                visible: parent.hovered
                delay: Qt.styleHints.mousePressAndHoldInterval
                text: qsTr("Show more actions")
                contentItem: Label {
                    text: moreActionsButtonTooltip.text
                    color: Style.ncTextColor
                }
                background: Rectangle {
                    border.color: Style.menuBorder
                    color: Style.backgroundColor
                }
            }

            Accessible.name: qsTr("Show more actions")

            onClicked:  moreActionsButtonContextMenu.popup(moreActionsButton.x, moreActionsButton.y);

            Connections {
                target: root.flickable

                function onMovementStarted() {
                    moreActionsButtonContextMenu.close();
                }
            }

            ActivityItemContextMenu {
                id: moreActionsButtonContextMenu

                maxActionButtons: root.maxActionButtons
                linksContextMenu: root.linksContextMenu

                onMenuEntryTriggered: function(entryIndex) {
                    root.triggerAction(entryIndex)
                }
            }
        }
    }
}
