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

#ifndef OWNCLOUD_THEME_H
#define OWNCLOUD_THEME_H

#include "theme.h"

namespace OCC {

class ownCloudTheme : public Theme
{
    Q_OBJECT
public:
    ownCloudTheme();

    QString configFileName() const Q_DECL_OVERRIDE;
    QString about() const Q_DECL_OVERRIDE;

#ifndef TOKEN_AUTH_ONLY
    QIcon   trayFolderIcon( const QString& ) const Q_DECL_OVERRIDE;
    QIcon   applicationIcon() const Q_DECL_OVERRIDE;
#endif
    QString appName() const Q_DECL_OVERRIDE;
    QString appNameGUI() const Q_DECL_OVERRIDE;

    QString helpUrl() const Q_DECL_OVERRIDE;
#ifndef TOKEN_AUTH_ONLY
    QVariant customMedia(CustomMediaType type) Q_DECL_OVERRIDE;

    QColor  wizardHeaderBackgroundColor() const Q_DECL_OVERRIDE;
    QColor  wizardHeaderTitleColor() const Q_DECL_OVERRIDE;
    QPixmap wizardHeaderLogo() const Q_DECL_OVERRIDE;
#endif
private:


};

}
#endif // OWNCLOUD_MIRALL_THEME_H
