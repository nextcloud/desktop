/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#pragma once

#include "owncloudlib.h"

#include "libsync/accountfwd.h"

#include <OAIDrive.h>

#include <QIcon>

namespace OCC {
namespace GraphApi {
    class SpacesManager;

    class OWNCLOUDSYNC_EXPORT Space : public QObject
    {
        Q_OBJECT
    public:
        /***
         * Returns the display name of the drive.
         * This is identical to drive.getName() for most drives.
         * Exceptions: Personal spaces
         */
        QString displayName() const;


        OpenAPI::OAIDrive drive() const;

        /***
         * Asign a priority to a drive, used for sorting
         */
        uint32_t priority() const;

        /**
         * Whether a drive object has been deleted.
         */
        bool disabled() const;

        /**
         * The image url
         */
        QUrl imageUrl() const;

        QIcon image() const;

    private:
        Space(SpacesManager *spaceManager, const OpenAPI::OAIDrive &drive);
        void setDrive(const OpenAPI::OAIDrive &drive);

        SpacesManager *_spaceManager;
        OpenAPI::OAIDrive _drive;

        QIcon _image;

        friend class SpacesManager;
    };

} // OCC
} // GraphApi
