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
class QPixmap;

namespace Mirall {

class SyncResult;

class Theme
{
public:
    enum CustomMediaType {
        oCSetupTop,      // ownCloud connect page
        oCSetupSide,
        oCSetupBottom,
        oCSetupResultTop // ownCloud connect result page
    };

    static Theme* instance();

    virtual QString appName() const = 0;

    virtual QString configFileName() const = 0;

    /**
      * get a folder icon for a given backend in a given size.
      */
    virtual QIcon   folderIcon( const QString& ) const = 0;

    /**
      * the icon that is shown in the tray context menu left of the folder name
      */
    virtual QIcon   trayFolderIcon( const QString& ) const;

    /**
      * get an sync state icon
      */
    virtual QIcon   syncStateIcon( SyncResult::Status, bool sysTray = false ) const = 0;

    virtual QIcon   folderDisabledIcon() const = 0;
    virtual QPixmap splashScreen() const = 0;

    virtual QIcon   applicationIcon() const = 0;

    virtual QString statusHeaderText( SyncResult::Status ) const;
    virtual QString version() const;

    /**
     * Characteristics: bool if more than one sync folder is allowed
     */
    virtual bool singleSyncFolder() const;

    /**
     * Setting a value here will pre-define the server url.
     *
     * The respective UI controls will be disabled
     */
    virtual QString overrideServerUrl() const;

    /**
     * The default folder name without path on the server at setup time.
     */
    virtual QString defaultServerFolder() const;

    /**
     * The default folder name without path on the client side at setup time.
     */
    virtual QString defaultClientFolder() const;

    /**
     * Override to encforce a particular locale, i.e. "de" or "pt_BR"
     */
    virtual QString enforcedLocale() const { return QString::null; }


    /**
     * Override to use a string or a custom image name.
     * The default implementation will try to look up
     * :/mirall/theme/<type>.png
     */
    virtual QVariant customMedia( CustomMediaType type );

    /**
     * About dialog contents
     */
    virtual QString about() const;

    /**
     * Define if the systray icons should be using mono design
     */
    void setSystrayUseMonoIcons(bool mono);

    /**
     * Retrieve wether to use mono icons for systray
     */
    bool systrayUseMonoIcons() const;

protected:
    QIcon themeIcon(const QString& name, bool sysTray = false) const;
    Theme() {}

private:
    Theme(Theme const&);
    Theme& operator=(Theme const&);

    static Theme* _instance;
    bool _mono;

};

}
#endif // _THEME_H
