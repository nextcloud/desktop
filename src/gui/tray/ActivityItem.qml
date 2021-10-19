import QtQml 2.12
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2
import Style 1.0
import com.nextcloud.desktopclient 1.0

MouseArea {
    id: activityMouseArea

    readonly property int maxActionButtons: 2
    property Flickable flickable

    signal fileActivityButtonClicked(string absolutePath)

    enabled: (path !== "" || link !== "")
    hoverEnabled: true

    Rectangle {
        anchors.fill: parent
        color: (parent.containsMouse ? Style.lightHover : "transparent")
    }
        
    RowLayout {
        id: activityItem
        
        readonly property variant links: model.links
        
        readonly property int itemIndex: model.index
        
        width: activityMouseArea.width
        height: Style.trayWindowHeaderHeight
        spacing: 0
        
        Accessible.role: Accessible.ListItem
        Accessible.name: path !== "" ? qsTr("Open %1 locally").arg(displayPath)
                                     : message
        Accessible.onPressAction: activityMouseArea.clicked()   
        
        Image {
            id: activityIcon
            Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter
            Layout.leftMargin: 20
            Layout.preferredWidth: shareButton.icon.width
            Layout.preferredHeight: shareButton.icon.height
            verticalAlignment: Qt.AlignCenter
            cache: true
            source: icon
            sourceSize.height: 64
            sourceSize.width: 64
        }
        
        Column {
            id: activityTextColumn
            Layout.leftMargin: 14
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            Layout.fillWidth: true
            spacing: 4
            Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
            
            Text {
                id: activityTextTitle
                text: (type === "Activity" || type === "Notification") ? subject : message
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: Style.topLinePixelSize
                color: activityTextTitleColor
            }
            
            Text {
                id: activityTextInfo
                text: (type === "Sync") ? displayPath
                                        : (type === "File") ? subject
                                                            : (type === "Notification") ? message
                                                                                        : ""
                height: (text === "") ? 0 : activityTextTitle.height
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: Style.subLinePixelSize
            }
            
            Text {
                id: activityTextDateTime
                text: dateTime
                height: (text === "") ? 0 : activityTextTitle.height
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: Style.subLinePixelSize
                color: "#808080"
            }
        }
        
        RowLayout {
            id: activityActionsLayout
            spacing: 0
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.minimumWidth: 28
            Layout.fillWidth: true
            
            function actionButtonIcon(actionIndex) {
                const verb = String(model.links[actionIndex].verb);
                if (verb === "WEB" && (model.objectType === "chat" || model.objectType === "call")) {
                    return "qrc:///client/theme/reply.svg";
                } else if (verb === "DELETE") {
                    return "qrc:///client/theme/close.svg";
                }
                
                return "qrc:///client/theme/confirm.svg";
            }
            
            Repeater {
                model: activityItem.links.length > maxActionButtons ? 1 : activityItem.links.length
                
                ActivityActionButton {
                    id: activityActionButton
                    
                    readonly property int actionIndex: model.index
                    readonly property bool primary: model.index === 0 && String(activityItem.links[actionIndex].verb) !== "DELETE"
                    
                    Layout.fillHeight: true                    
                    
                    text: !primary ? "" : activityItem.links[actionIndex].label
                    
                    imageSource: !primary ? activityActionsLayout.actionButtonIcon(actionIndex) : ""
                    
                    textColor: primary ? Style.ncBlue : "black"
                    textColorHovered: Style.lightHover
                    
                    textBorderColor: Style.ncBlue
                    
                    textBgColor: "transparent"
                    textBgColorHovered: Style.ncBlue
                    
                    tooltipText: activityItem.links[actionIndex].label
                    
                    Layout.minimumWidth: primary ? 80 : -1
                    Layout.minimumHeight: parent.height
                    
                    Layout.preferredWidth: primary ? -1 : parent.height
                    
                    onClicked: activityModel.triggerAction(activityItem.itemIndex, actionIndex)
                }
                
            }

            Button {
                id: shareButton
                
                Layout.preferredWidth:  parent.height
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignRight
                flat: true
                hoverEnabled: true
                visible: displayActions && (path !== "")
                display: AbstractButton.IconOnly
                icon.source: "qrc:///client/theme/share.svg"
                icon.color: "transparent"
                background: Rectangle {
                    color: parent.hovered ? Style.lightHover : "transparent"
                }
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: qsTr("Open share dialog")
                onClicked: Systray.openShareDialog(displayPath, absolutePath)
                
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Share %1").arg(displayPath)
                Accessible.onPressAction: shareButton.clicked()
            }
            
            Button {
                id: moreActionsButton
                
                Layout.preferredWidth: parent.height
                Layout.preferredHeight: parent.height
                Layout.alignment: Qt.AlignRight
                
                flat: true
                hoverEnabled: true
                visible: displayActions && ((path !== "") || (activityItem.links.length > maxActionButtons))
                display: AbstractButton.IconOnly
                icon.source: "qrc:///client/theme/more.svg"
                icon.color: "transparent"
                background: Rectangle {
                    color: parent.hovered ? Style.lightHover : "transparent"
                }
                ToolTip.visible: hovered
                ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
                ToolTip.text: qsTr("Show more actions")
                
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Show more actions")
                Accessible.onPressAction: moreActionsButton.clicked()
                
                onClicked:  moreActionsButtonContextMenu.popup();
                
                Connections {
                    target: flickable
                    
                    function onMovementStarted() {
                        moreActionsButtonContextMenu.close();
                    }
                }
                
                Container {
                    id: moreActionsButtonContextMenuContainer
                    visible: moreActionsButtonContextMenu.opened
                    
                    width: moreActionsButtonContextMenu.width
                    height: moreActionsButtonContextMenu.height
                    anchors.right: moreActionsButton.right
                    anchors.top: moreActionsButton.top
                    
                    AutoSizingMenu {
                        id: moreActionsButtonContextMenu
                        anchors.centerIn: parent
                        
                        // transform model to contain indexed actions with primary action filtered out
                        function actionListToContextMenuList(actionList) {
                            // early out with non-altered data
                            if (activityItem.links.length <= maxActionButtons) {
                                return actionList;
                            }
                            
                            // add index to every action and filter 'primary' action out
                            var reducedActionList = actionList.reduce(function(reduced, action, index) {
                                if (!action.primary) {
                                    var actionWithIndex = { actionIndex: index, label: action.label };
                                    reduced.push(actionWithIndex);
                                }
                                return reduced;
                            }, []);
                            
                            
                            return reducedActionList;
                        }

                        MenuItem {
                            text: qsTr("View activity")
                            onClicked: fileActivityButtonClicked(absolutePath)
                        }
                        
                        Repeater {
                            id: moreActionsButtonContextMenuRepeater
                            
                            model: moreActionsButtonContextMenu.actionListToContextMenuList(activityItem.links)
                            
                            delegate: MenuItem {
                                id: moreActionsButtonContextMenuEntry
                                text: model.modelData.label
                                onTriggered: activityModel.triggerAction(activityItem.itemIndex, model.modelData.actionIndex)
                            }
                        }
                    }
                }
            }
        }
    }
}
