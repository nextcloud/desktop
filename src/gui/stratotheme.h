#ifndef _STRATOTHEME_H
#define _STRATOTHEME_H

#include <QFont>
#include <QString>
#include "basetheme.h"
#include "theme.h"

namespace OCC {

class StratoTheme : public BaseTheme {
public:
    StratoTheme() = default;

    QString additionalThemePrefix() const override { return QStringLiteral("strato/"); }

    QString dialogBackgroundColor() const override {
        return themedColor("#F7F7F9", "#1F2024");
    }

    QString trayFontColor() const override {
        return themedColor("#2F2F70", "#C9CBEF");
    }

    QString trayBorderColor() const override {
        return themedColor("#8493B3", "#454C5E");
    }

    QString trayInputFieldBorderColor() const override {
        return themedColor("#8493B3", "#454C5E");
    }

    QString trayBackgroundColor() const override {
        return themedColor("#F7F7F9", "#1F2024");
    }

    QString settingsLinkColor() const override {
        return themedColor("#272CB2", "#5B60D6");
    }

    QString quotaProgressColor() const override {
        return themedColor("#272CB2", "#5B60D6");
    }

    QString syncProgressColor() const override {
        return "#009850";
    }

    QString buttonPrimaryColor() const override {
        // Dark value is IONOS Figma token Color/Blue Ionos/B4 (STRUXD-157).
        return themedColor("#272CB2", "#1474C4");
    }

    QString buttonPrimaryHoverColor() const override {
        // TODO: no distinct dark hover value confirmed in the IONOS spec (STRUXD-157 only
        // showed default/pressed/disabled) - using the default color as an interim value.
        return themedColor("#2944CC", "#1474C4");
    }

    QString buttonPrimaryPressedColor() const override {
        // Dark value is IONOS Figma token Color/Blue Ionos/B5 (STRUXD-157) - deliberately
        // darker than the default, not lighter.
        return themedColor("#272CB2", "#095BB1");
    }

    QString buttonPrimaryFocusedBorderColor() const override {
        return themedColor("#CDD5E3", "#FFFFFF");
    }

    QString buttonSecondaryColor() const override {
        // Dark value matches pillButtonSecondaryColor() below (SecondaryPillButton.qml).
        return themedColor("#F7F7F9", "#2E2F3D");
    }

    QString buttonSecondaryBorderColor() const override {
        // Dark value matches pillButtonBorderColor() below, which stays white in both modes.
        return themedColor("#CDD5E3", "#FFFFFF");
    }

    QString buttonSecondaryHoverColor() const override {
        return themedColor("#EDEEF3", "#282A36");
    }

    QString buttonSecondaryPressedColor() const override {
        return themedColor("#D6D6E4", "#3A3B52");
    }

    // Blended halfway toward buttonSecondaryBorderColor() (the already-bright default
    // border) instead of the previous, more saturated blue-grey - autoDefault buttons keep
    // focus after a click (normal Qt behavior), so a strongly-colored focus ring made them
    // read as permanently "selected" rather than a subtle focus cue. No confirmed design
    // token for this - a maintained approximation, not a literal spec value.
    QString buttonSecondaryFocusedBorderColor() const override {
        return themedColor("#A9B4CB", "#A2A6AF");
    }

    QString buttonDisabledColor() const override {
        return themedColor("#EDEEF3", "#282A36");
    }

    QString pillButtonPrimaryColor() const override {
        // Kept in sync with buttonPrimaryColor() above (IONOS Blue Ionos/B4 in dark mode).
        return themedColor("#272CB2", "#1474C4");
    }

    QString pillButtonSecondaryColor() const override {
        return themedColor("#E4E4ED", "#2E2F3D");
    }

    QString pillButtonBorderColor() const override {
        return "#FFFFFF";
    }

    QString clipboardBackgroundColor() const override {
        return "#f7f7f9";
    }

    QString buttonIconColor() const override {
        return themedColor("#2f2f70", "#C9CBEF");
    }

    QString buttonIconHoverColor() const override {
        return themedColor("#2f2f70", "#C9CBEF");
    }

    QString buttonHoveredColor() const override {
        return themedColor("#eeeff9", "#2A2B3D");
    }

    QString buttonPressedColor() const override {
        return themedColor("#D6D6E4", "#3A3B52");
    }

    QString toolButtonHoveredColor() const override {
        return themedColor("#EDEEF3", "#282A36");
    }

    QString toolButtonPressedColor() const override {
        return themedColor("#D6D6E4", "#3A3B52");
    }

    QString menuTextColor() const override {
        return themedColor("#29294d", "#C9CBEF");
    }

    QString menuSelectedItemColor() const override {
        return themedColor("#D6D6E4", "#282A36");
    }

    QString menuPressedTextColor() const override {
        return "#FFFFFF";
    }

    QString iconDarkColor() const override {
        return themedColor("#2F2F70", "#C9CBEF");
    }

    QString menuPressedItemColor() const override {
        return themedColor("#5A6782", "#3A3B52");
    }

    QString errorBorderColor() const override {
        return themedColor("#FF004C", "#FF6688");
    }

    QString trayErrorBorderColor() const override {
        return themedColor("#FF004C", "#FF6688");
    }

    QString trayErrorTextColor() const override {
        return themedColor("#CC0052", "#FF8FB3");
    }

    QString sesHeaderLogoIcon() const override {
        return QStringLiteral("qrc:///client/theme/ses/strato/ses-STRATO-logo.svg");
    }
};
}
#endif // _STRATOTHEME_H