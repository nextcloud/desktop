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

#ifndef MIRALL_WIZARD_SHIBBOLETH_COOKIE_JAR_H
#define MIRALL_WIZARD_SHIBBOLETH_COOKIE_JAR_H

#include <QNetworkCookieJar>
#include <QList>

class QUrl;
class QNetworkCookie;

namespace Mirall
{

class ShibbolethCookieJar : public QNetworkCookieJar
{
  Q_OBJECT

public:
  ShibbolethCookieJar (QObject* parent = 0);

  virtual bool setCookiesFromUrl (const QList<QNetworkCookie>& cookieList, const QUrl& url);

Q_SIGNALS:
  void newCookiesForUrl (const QList<QNetworkCookie>& cookieList, const QUrl& url);
};

} // ns Mirall

#endif
