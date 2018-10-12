/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_COOKIEJAR_H
#define MIRALL_COOKIEJAR_H

#include <QNetworkCookieJar>

#include "owncloudlib.h"

namespace OCC {

/**
 * @brief The CookieJar class
 * @ingroup libsync
 */
class OWNCLOUDSYNC_EXPORT CookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit CookieJar(QObject *parent = Q_NULLPTR);
    ~CookieJar();
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) Q_DECL_OVERRIDE;
    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const Q_DECL_OVERRIDE;

    void clearSessionCookies();

    using QNetworkCookieJar::setAllCookies;
    using QNetworkCookieJar::allCookies;

    void save(const QString &fileName);
    void restore(const QString &fileName);

signals:
    void newCookiesForUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);

private:
    QList<QNetworkCookie> removeExpired(const QList<QNetworkCookie> &cookies);
};

} // namespace OCC

#endif // MIRALL_COOKIEJAR_H
