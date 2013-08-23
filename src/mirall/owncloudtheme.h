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

namespace Mirall {

class ownCloudTheme : public Theme
{
    Q_OBJECT
public:
    ownCloudTheme();

    QString configFileName() const;
    QString about() const;
    QPixmap splashScreen() const;

    QIcon   folderIcon( const QString& ) const;
    QIcon   trayFolderIcon( const QString& ) const;
    QIcon   folderDisabledIcon() const;
    QIcon   applicationIcon() const;

    QVariant customMedia(CustomMediaType type);
    QString helpUrl() const;

    QColor  wizardHeaderBackgroundColor() const;
    QColor  wizardHeaderTitleColor() const;
    QPixmap wizardHeaderLogo() const;
private:


};

}
#endif // OWNCLOUD_MIRALL_THEME_H
