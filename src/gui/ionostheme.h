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
};
}
#endif // _IONOSTHEME_H