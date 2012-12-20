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

#ifndef MIRALL_THEME_H
#define MIRALL_THEME_H

#include "theme.h"

namespace Mirall {

class mirallTheme : public Theme
{
public:
    mirallTheme();

    virtual QString appName() const;
    virtual QString configFileName() const;
    QPixmap splashScreen() const;

    QIcon   folderIcon( const QString& ) const;
    QIcon   syncStateIcon(SyncResult::Status, bool) const;
    QIcon   folderDisabledIcon() const;
    QIcon   applicationIcon() const;

private:


};

}
#endif // MIRALL_THEME_H
