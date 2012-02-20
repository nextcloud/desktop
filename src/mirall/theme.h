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

#ifndef _THEME_H
#define _THEME_H

#include "mirall/syncresult.h"

class QIcon;
class QString;
class QObject;

namespace Mirall {

class SyncResult;

class Theme : public QObject
{
public:
    Theme();

    virtual QString appName() const = 0;

    virtual QString configFileName() const = 0;

    /**
      * get a folder icon for a given backend in a given size.
      */
    QIcon folderIcon( const QString&, int ) const;
    QIcon syncStateIcon( SyncResult::Status, int ) const;
    QString statusHeaderText( SyncResult::Status ) const;

private:


};

}
#endif // _THEME_H
