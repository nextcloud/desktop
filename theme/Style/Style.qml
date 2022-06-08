pragma Singleton

import QtQuick 2.15

import com.nextcloud.desktopclient 1.0

QtObject {
    readonly property int pixelSize: fontMetrics.font.pixelSize

    // Colors
    readonly property color ncBlue:      Theme.wizardHeaderBackgroundColor
    readonly property color ncTextColor: Theme.systemPalette.windowText
    readonly property color ncSecondaryTextColor: "#808080"
    readonly property color ncHeaderTextColor: "white"
    readonly property color lightHover: Theme.darkMode ? Qt.lighter(backgroundColor, 2) : Qt.darker(backgroundColor, 1.05)
    readonly property color menuBorder: Theme.darkMode ? Qt.lighter(backgroundColor, 2.5) : Qt.darker(backgroundColor, 1.5)
    readonly property color backgroundColor: Theme.systemPalette.base
    readonly property color buttonBackgroundColor: Theme.systemPalette.button

    // ErrorBox colors
    readonly property color errorBoxTextColor:       Theme.errorBoxTextColor
    readonly property color errorBoxBackgroundColor: Theme.errorBoxBackgroundColor
    readonly property color errorBoxBorderColor:     Theme.errorBoxBorderColor

    // Fonts
    // We are using pixel size because this is cross platform comparable, point size isn't
    readonly property int topLinePixelSize: pixelSize
    readonly property int subLinePixelSize: topLinePixelSize - 2

    // Dimensions and sizes
    property int trayWindowWidth: variableSize(400)
    property int trayWindowHeight: variableSize(510)
    property int trayWindowRadius: 10
    property int trayWindowBorderWidth: variableSize(1)
    property int trayWindowHeaderHeight: variableSize(60)
    property int trayHorizontalMargin: 10
    property int trayListItemIconSize: accountAvatarSize
    property real thumbnailImageSizeReduction: 0.2  // We reserve some space within the thumbnail "item", here about 20%.
                                                    // This is because we need to also add the added/modified icon and we
                                                    // want them to fit within the general icon size. We also need to know
                                                    // this amount to properly center the sync status icon to the thumbnail
                                                    // images, which will work so long as the thumbnails are left aligned

    property int standardSpacing: 10

    property int minActivityHeight: variableSize(40)

    property int currentAccountButtonWidth: 220
    property int currentAccountButtonRadius: 2
    property int currentAccountLabelWidth: 128

    property int normalBorderWidth: 1
    property int thickBorderWidth: 2
    property int veryRoundedButtonRadius: 100
    property int mediumRoundedButtonRadius: 8
    property int slightlyRoundedButtonRadius: 5
    property double hoverOpacity: 0.7

    property url stateOnlineImageSource: Theme.stateOnlineImageSource
    property url stateOfflineImageSource: Theme.stateOfflineImageSource

    property int accountAvatarSize: (trayWindowHeaderHeight - 16)
    property int accountAvatarStateIndicatorSize: 16
    property int folderStateIndicatorSize: 16
    property int accountLabelWidth: 128

    property int accountDropDownCaretSize: 20
    property int accountDropDownCaretMargin: 8

    property int addAccountButtonHeight: 50

    property int headerButtonIconSize: 32

    property int activityLabelBaseWidth: 240

    property int activityItemActionPrimaryButtonMinWidth: 100
    property int activityItemActionSecondaryButtonMinWidth: 80

    property int callNotificationPrimaryButtonMinWidth: 100
    property int callNotificationPrimaryButtonMinHeight: 40

    property int roundButtonBackgroundVerticalMargins: 10
    property int roundedButtonBackgroundVerticalMargins: 5
    
    property int userStatusEmojiSize: 8
    property int userStatusSpacing: trayHorizontalMargin
    property int userStatusAnchorsMargin: 2
    property int accountServerAnchorsMargin: 10
    property int accountLabelsSpacing: 4
    property int accountLabelsAnchorsMargin: 7
    property int accountLabelsLayoutMargin: 12
    property int accountLabelsLayoutTopMargin: 10

    // Visual behaviour
    property bool hoverEffectsEnabled: true

    // unified search constants
    readonly property int unifiedSearchItemHeight: trayWindowHeaderHeight
    readonly property int unifiedSearchResultTextLeftMargin: 18
    readonly property int unifiedSearchResultTextRightMargin: 16
    readonly property int unifiedSearchResulIconWidth: 24
    readonly property int unifiedSearchResulIconLeftMargin: 12
    readonly property int unifiedSearchResulTitleFontSize: topLinePixelSize
    readonly property int unifiedSearchResulSublineFontSize: subLinePixelSize

    readonly property var fontMetrics: FontMetrics {}

    readonly property int activityContentSpace: 4

    function variableSize(size) {
        return size * (1 + Math.min(pixelSize / 100, 1));       
    }
}
