/*
 * Copyright (C) Hannah von Reth <hannah.vonreth@owncloud.com>
 * Copyright (C) Fabian Müller <fmueller@owncloud.com>
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

#include "abstractcorejob.h"

#include "networkjobs.h"
#include "owncloudlib.h"

namespace OCC {


class OWNCLOUDSYNC_EXPORT DetermineAuthTypeJobFactory : public AbstractCoreJobFactory
{
public:
    using AuthType = DetermineAuthTypeJob::AuthType;

    DetermineAuthTypeJobFactory(QNetworkAccessManager *nam);
    ~DetermineAuthTypeJobFactory() override;

    CoreJob *startJob(const QUrl &url, QObject *parent) override;
};
}
