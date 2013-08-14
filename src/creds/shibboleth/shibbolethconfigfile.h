/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#ifndef MIRALL_CREDS_SHIBBOLETH_CONFIG_FILE_H
#define MIRALL_CREDS_SHIBBOLETH_CONFIG_FILE_H

#include <QList>
#include <QMap>
#include <QNetworkCookie>
#include <QUrl>

#include "mirall/mirallconfigfile.h"

namespace Mirall
{

class ShibbolethCookieJar;

class ShibbolethConfigFile : public MirallConfigFile
{
public:
    void storeCookies(const QMap<QUrl, QList<QNetworkCookie> >& cookies);
    ShibbolethCookieJar* createCookieJar() const;
};

} // ns Mirall

#endif
