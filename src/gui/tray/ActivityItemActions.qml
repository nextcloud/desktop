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
    signal showReplyField()

    Repeater {
        id: actionsRepeater
        // a max of maxActionButtons will get dispayed as separate buttons
        model: root.linksForActionButtons

        ActivityActionButton {
            id: activityActionButton

            Layout.minimumWidth: primaryButton ? Style.activityItemActionPrimaryButtonMinWidth : Style.activityItemActionSecondaryButtonMinWidth
            Layout.preferredHeight: parent.height
            Layout.preferredWidth: primaryButton ? -1 : parent.height

            verb: model.modelData.verb
            primaryButton: (model.index === 0 && verb !== "DELETE") || model.modelData.primary
            isTalkReplyButton: verb === "REPLY"

            text: model.modelData.label

            adjustedHeaderColor: Style.adjustedCurrentUserHeaderColor

            imageSource: model.modelData.imageSource ? model.modelData.imageSource + Style.adjustedCurrentUserHeaderColor : ""
            imageSourceHover: model.modelData.imageSourceHovered ? model.modelData.imageSourceHovered + Style.currentUserHeaderTextColor : ""

            onClicked: isTalkReplyButton ? root.showReplyField() : root.triggerAction(model.index)
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

            NCToolTip {
                visible: parent.hovered
                text: qsTr("Show more actions")
            }

            Accessible.name: qsTr("Show more actions")

            onClicked:  moreActionsButtonContextMenu.popup(moreActionsButton.x, moreActionsButton.y);

            Connections {
                target: root.flickable

                function onMovementStarted() {
                    moreActionsButtonContextMenu.close();
                }
            }
        }
    }
}
