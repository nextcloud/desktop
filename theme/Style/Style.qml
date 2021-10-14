pragma Singleton

import QtQuick 2.15

import com.nextcloud.desktopclient 1.0

Item {
    readonly property int pixelSize: fontMetrics.font.pixelSize

    // Colors
    property color ncBlue:      Theme.wizardHeaderBackgroundColor
    property color ncTextColor: Theme.wizardHeaderTitleColor
    property color lightHover:  "#f7f7f7"
    property color menuBorder:  "#bdbdbd"

    // ErrorBox colors
    property color errorBoxTextColor:       Theme.errorBoxTextColor
    property color errorBoxBackgroundColor: Theme.errorBoxBackgroundColor
    property color errorBoxBorderColor:     Theme.errorBoxBorderColor

    // Fonts
    // We are using pixel size because this is cross platform comparable, point size isn't
    readonly property int topLinePixelSize: pixelSize
    readonly property int subLinePixelSize: topLinePixelSize - 2

    // Dimensions and sizes
    property int trayWindowWidth: variableSize(400)
    property int trayWindowHeight: variableSize(510)
    property int trayWindowRadius: 10
    property int trayWindowBorderWidth: 1
    property int trayWindowHeaderHeight: variableSize(60)

    property int currentAccountButtonWidth: 220
    property int currentAccountButtonRadius: 2
    property int currentAccountLabelWidth: 128

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
    
    property int userStatusEmojiSize: 8
    property int userStatusSpacing: 6
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
    readonly property string unifiedSearchResulTitleColor: "black"
    readonly property string unifiedSearchResulSublineColor: "grey"

    function variableSize(size) {
        return size * (1 + Math.min(pixelSize / 100, 1));       
    }

    FontMetrics {
        id: fontMetrics
    }
}
