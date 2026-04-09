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
        return "#F7F7F9";
    }

    QString trayFontColor() const override {
        return "#2F2F70";
    }

    QString trayBorderColor() const override {
        return "#8493B3";
    }

    QString trayInputFieldBorderColor() const override {
        return "#8493B3";
    }

    QString trayBackgroundColor() const override {
        return "#F7F7F9";
    }

    QString settingsLinkColor() const override {
        return "#272CB2";
    }

    QString quotaProgressColor() const override {
        return "#272CB2";
    }

    QString syncProgressColor() const override {
        return "#009850";
    }

    QString buttonPrimaryColor() const override { 
        return "#272CB2";
    }

    QString buttonPrimaryHoverColor() const override {
        return "#2944CC";
    }

    QString buttonPrimaryPressedColor() const override {
        return "#272CB2";
    }

    QString buttonPrimaryFocusedBorderColor() const override {
        return "#CDD5E3";
    }

    QString buttonSecondaryColor() const override { 
        return "#F7F7F9";
    }

    QString buttonSecondaryBorderColor() const override { 
        return "#CDD5E3";
    }

    QString buttonSecondaryHoverColor() const override {
        return "#EDEEF3";
    }

    QString buttonSecondaryPressedColor() const override {
        return "#D6D6E4";
    }

    QString buttonSecondaryFocusedBorderColor() const override {
        return "#8493B3";
    }

    QString buttonDisabledColor() const override {
        return "#EDEEF3";
    }

    QString pillButtonPrimaryColor() const override {
        return "#272CB2";
    }

    QString pillButtonSecondaryColor() const override {
        return "#E4E4ED";
    }

    QString pillButtonBorderColor() const override {
        return "#FFFFFF";
    }

    QString clipboardBackgroundColor() const override {
        return "#f7f7f9";
    }

    QString buttonIconColor() const override {
        return "#2f2f70";
    }

    QString buttonIconHoverColor() const override {
        return "#2f2f70";
    }

    QString buttonHoveredColor() const override {
        return "#eeeff9";
    }

    QString buttonPressedColor() const override {
        return "#D6D6E4";
    }

    QString toolButtonHoveredColor() const override {
        return "#EDEEF3";
    }

    QString toolButtonPressedColor() const override {
        return "#D6D6E4";
    }

    QString menuTextColor() const override {
        return "#29294d";
    }

    QString menuSelectedItemColor() const override {
        return "#D6D6E4";
    }

    QString menuPressedTextColor() const override {
        return "#FFFFFF";
    }

    QString iconDarkColor() const override {
        return "#2F2F70";
    }

    QString menuPressedItemColor() const override {
        return "#5A6782";
    }

    QString errorColor() const override {
        return "#FFE0ED";
    }

    QString errorBorderColor() const override {
        return "#FF004C";
    }

    QString trayErrorBorderColor() const override {
        return "#FF004C";
    }

    QString trayErrorTextColor() const override {
        return "#CC0052";
    }

    QString sesHeaderLogoIcon() const override {
        return QStringLiteral("qrc:///client/theme/ses/strato/ses-STRATO-logo.svg");
    }
};
}
#endif // _STRATOTHEME_H