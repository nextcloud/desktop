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

    property int maxActionButtons: 0

    property Flickable flickable

    signal triggerAction(int actionIndex)

    Repeater {
        id: actionsRepeater
        // a max of maxActionButtons will get dispayed as separate buttons
        model: root.linksForActionButtons

        ActivityActionButton {
            id: activityActionButton

            readonly property bool primary: model.index === 0 && model.modelData.verb !== "DELETE"

            Layout.minimumWidth: primary ? Style.activityItemActionPrimaryButtonMinWidth : Style.activityItemActionSecondaryButtonMinWidth
            Layout.preferredHeight: primary ? parent.height : parent.height * 0.3
            Layout.preferredWidth: primary ? -1 : parent.height

            text: model.modelData.label
            toolTipText: model.modelData.label

            imageSource: model.modelData.imageSource
            imageSourceHover: model.modelData.imageSourceHovered

            textColor: imageSource !== "" ? UserModel.currentUser.headerColor : Style.unifiedSearchResulSublineColor
            textColorHovered: imageSource !== "" ? Style.lightHover : Style.unifiedSearchResulTitleColor

            bold: primary

            onClicked: root.triggerAction(model.index)
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

            background: Rectangle {
                color: parent.hovered ? "white" : root.moreActionsButtonColor
                radius: width / 2
            }

            ToolTip.visible: hovered
            ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            ToolTip.text: qsTr("Show more actions")

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
