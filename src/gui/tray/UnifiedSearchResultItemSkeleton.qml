import QtQml 2.15
import QtQuick 2.15
import QtQuick.Layouts 1.2
import QtGraphicalEffects 1.15
import Style 1.0

RowLayout {
    id: unifiedSearchResultSkeletonItemDetails

    property int textLeftMargin: Style.unifiedSearchResultTextLeftMargin
    property int textRightMargin: Style.unifiedSearchResultTextRightMargin
    property int iconWidth: Style.unifiedSearchResultIconWidth
    property int iconLeftMargin: Style.unifiedSearchResultIconLeftMargin

    property int titleFontSize: Style.unifiedSearchResultTitleFontSize
    property int sublineFontSize: Style.unifiedSearchResultSublineFontSize

    Accessible.role: Accessible.ListItem
    Accessible.name: qsTr("Search result skeleton.").arg(model.index)

    height: Style.trayWindowHeaderHeight

    /*
    * An overview of what goes on here:
    *
    * We want to provide the user with a loading animation of a unified search result list item "skeleton".
    * This skeleton has a square and two rectangles that are meant to represent the icon and the text of a
    * real search result.
    *
    * We also want to provide a nice animation that has a dark gradient moving over these shapes. To do so,
    * we very simply create rectangles that have a gradient going transparent->dark->dark->transparent, and
    * overlay them over the shapes beneath.
    *
    * We then use NumberAnimations to move these gradients left-to-right over the base shapes below. Since
    * the gradient rectangles are child elements of the base-color rectangles which they move over, as long
    * as we ensure that the parent rectangle has the "clip" property enabled this will look nice and won't
    * spill over onto other elements.
    *
    * We also want to make sure that, even though these gradient rectangles are separate, it looks as it is
    * one single gradient sweeping over the base color components
    */

    property color baseGradientColor: Style.lightHover
    property color progressGradientColor: Style.darkMode ? Qt.lighter(baseGradientColor, 1.2) : Qt.darker(baseGradientColor, 1.1)

    property int animationRectangleWidth: Style.trayWindowWidth
    property int animationStartX: -animationRectangleWidth
    property int animationEndX: animationRectangleWidth

    Component {
        id: gradientAnimationRectangle
        Rectangle {
            width: unifiedSearchResultSkeletonItemDetails.animationRectangleWidth
            height: parent.height
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop {
                    position: 0
                    color: "transparent"
                }
                GradientStop {
                    position: 0.4
                    color: unifiedSearchResultSkeletonItemDetails.progressGradientColor
                }
                GradientStop {
                    position: 0.6
                    color: unifiedSearchResultSkeletonItemDetails.progressGradientColor
                }
                GradientStop {
                    position: 1.0
                    color: "transparent"
                }
            }

            NumberAnimation on x {
                from: unifiedSearchResultSkeletonItemDetails.animationStartX
                to: unifiedSearchResultSkeletonItemDetails.animationEndX
                duration: 1000
                loops: Animation.Infinite
                running: true
            }
        }
    }

    Item {
        Layout.preferredWidth: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.preferredHeight: unifiedSearchResultSkeletonItemDetails.iconWidth
        Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.iconLeftMargin
        Layout.alignment: Qt.AlignVCenter | Qt.AlignHCenter

        Rectangle {
            id: unifiedSearchResultSkeletonThumbnail
            anchors.fill: parent
            color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
            clip: true
            visible: false

            Loader {
                x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                height: parent.height
                sourceComponent: gradientAnimationRectangle
            }
        }

        Rectangle {
            id: unifiedSearchResultSkeletonThumbnailMask
            anchors.fill: unifiedSearchResultSkeletonThumbnail
            color: "white"
            radius: 100
            visible: false
        }

        OpacityMask {
            anchors.fill: unifiedSearchResultSkeletonThumbnail
            source: unifiedSearchResultSkeletonThumbnail
            maskSource: unifiedSearchResultSkeletonThumbnailMask
        }
    }

    Column {
        id: unifiedSearchResultSkeletonTextContainer

        Layout.fillWidth: true
        Layout.leftMargin: unifiedSearchResultSkeletonItemDetails.textLeftMargin
        Layout.rightMargin: unifiedSearchResultSkeletonItemDetails.textRightMargin

        spacing: Style.standardSpacing

        Item {
            height: unifiedSearchResultSkeletonItemDetails.titleFontSize
            width: parent.width

            Rectangle {
                id: unifiedSearchResultSkeletonTitleText
                anchors.fill: parent
                color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
                clip: true
                visible: false

                Loader {
                    x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                    height: parent.height
                    sourceComponent: gradientAnimationRectangle
                }
            }

            Rectangle {
                id: unifiedSearchResultSkeletonTitleTextMask
                anchors.fill: unifiedSearchResultSkeletonTitleText
                color: "white"
                radius: Style.veryRoundedButtonRadius
                visible: false
            }

            OpacityMask {
                anchors.fill: unifiedSearchResultSkeletonTitleText
                source: unifiedSearchResultSkeletonTitleText
                maskSource: unifiedSearchResultSkeletonTitleTextMask
            }
        }

        Item {
            height: unifiedSearchResultSkeletonItemDetails.sublineFontSize
            width: parent.width

            Rectangle {
                id: unifiedSearchResultSkeletonTextSubline
                anchors.fill: parent
                color: unifiedSearchResultSkeletonItemDetails.baseGradientColor
                clip: true
                visible: false

                Loader {
                    x: mapFromItem(unifiedSearchResultSkeletonItemDetails, 0, 0).x
                    height: parent.height
                    sourceComponent: gradientAnimationRectangle
                }
            }

            Rectangle {
                id: unifiedSearchResultSkeletonTextSublineMask
                anchors.fill: unifiedSearchResultSkeletonTextSubline
                color: "white"
                radius: Style.veryRoundedButtonRadius
                visible: false
            }

            OpacityMask {
                anchors.fill:unifiedSearchResultSkeletonTextSubline
                source: unifiedSearchResultSkeletonTextSubline
                maskSource: unifiedSearchResultSkeletonTextSublineMask
            }
        }
    }
}
