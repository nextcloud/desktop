/*
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

namespace OCC {

/**
 * Fetches the user name. For use during OAuth2 login process *only*.
 */
class DetermineUserJobFactory : public OCC::AbstractCoreJobFactory
{
    Q_OBJECT

public:
    explicit DetermineUserJobFactory(QNetworkAccessManager *networkAccessManager, const QString &accessToken, QObject *parent = nullptr);

    CoreJob *startJob(const QUrl &url) override;

private:
    QString _accessToken;
};

}
