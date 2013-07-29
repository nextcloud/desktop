/*
 * Copyright (C) by Krzesimir Nowak <krzesimir@endocode.com>
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

#include "mirall/creds/shibbolethcookiejar.h"

namespace Mirall
{

ShibbolethCookieJar::ShibbolethCookieJar (QObject* parent)
  : QNetworkCookieJar (parent)
{}

bool ShibbolethCookieJar::setCookiesFromUrl (const QList<QNetworkCookie>& cookieList, const QUrl& url)
{
  if (QNetworkCookieJar::setCookiesFromUrl (cookieList, url)) {
    Q_EMIT newCookiesForUrl (cookieList, url);

    return true;
  }

  return false;
}

} // ns Mirall
