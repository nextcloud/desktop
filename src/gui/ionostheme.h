#ifndef _IONOSTHEME_H
#define _IONOSTHEME_H

#include <QFont>
#include <QString>
#include "theme.h"
#include "basetheme.h"

namespace OCC {

class IonosTheme : public BaseTheme {
public:
    IonosTheme() = default;

     QString additionalThemePrefix() const override { return QStringLiteral(""); }

    QString sesHeaderLogoIcon() const override {
        return QStringLiteral("qrc:///client/theme/ses/ses-IONOS-logo.svg");
    }

    // Brand-blue variants (ses-IONOS-logo.svg uses #003D8F) instead of the generic navy/accent
    // blue the base theme uses, so recolored icons read as IONOS-branded rather than generic.
    // #5FA8E0 (hue ~206°) is the app's existing dark-mode tint for blue UI elements; it's close
    // enough in hue to #003D8F (~214°) to read as the same lightened-for-dark-backgrounds blue
    // rather than a second, unrelated accent color.
    QString iconDarkColor() const override {
        return themedColor("#003D8F", "#5FA8E0");
    }

    QString buttonIconColor() const override {
        return themedColor("#003D8F", "#5FA8E0");
    }

    // The logo (ses-IONOS-logo.svg) is a fixed brand blue, #003D8F, never recolored - any
    // banner background close to that hue/brightness fights it for contrast (tried #0B2A63
    // and #123B85, both washed the wordmark out). Matching the tray's own background instead
    // means the banner has no separate box at all - the logo carries the brand color by itself,
    // and there's no shade to keep in contrast with it.
    QString headerBannerColor() const override {
        return trayBackgroundColor();
    }
};
}
#endif // _IONOSTHEME_H