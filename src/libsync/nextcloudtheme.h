/*
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef NEXTCLOUD_THEME_H
#define NEXTCLOUD_THEME_H

#include "theme.h"

namespace OCC {

/**
 * @brief The NextcloudTheme class
 * @ingroup libsync
 */
class NextcloudTheme : public Theme
{
    Q_OBJECT
public:
    NextcloudTheme();

#ifndef TOKEN_AUTH_ONLY
    QVariant customMedia(CustomMediaType type) override;

    QColor wizardHeaderBackgroundColor() const override;
    QColor wizardHeaderTitleColor() const override;
    QPixmap wizardHeaderLogo() const override;
#endif

private:
};
}
#endif // NEXTCLOUD_THEME_H
