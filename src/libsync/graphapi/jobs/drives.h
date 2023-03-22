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

#include "networkjobs/jsonjob.h"

#include "owncloudlib.h"
#include <OAIDrive.h>

namespace OCC {
namespace GraphApi {


    class OWNCLOUDSYNC_EXPORT Drives : public JsonJob
    {
        Q_OBJECT
    public:
        Drives(const AccountPtr &account, QObject *parent = nullptr);
        ~Drives();
        const QList<OpenAPI::OAIDrive> &drives() const;

    private:
        mutable QList<OpenAPI::OAIDrive> _drives;
    };
}
}
