/*
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#ifndef MIRALL_COOKIEJAR_H
#define MIRALL_COOKIEJAR_H

#include <QNetworkCookieJar>

namespace Mirall {

class CookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit CookieJar(QObject *parent = 0);
    ~CookieJar();
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);
    QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const;

    virtual bool deleteCookie(const QNetworkCookie & cookie);

signals:
    void newCookiesForUrl(const QList<QNetworkCookie>& cookieList, const QUrl& url);
private:
    void save();
    void restore();
    QList<QNetworkCookie> removeExpired(const QList<QNetworkCookie> &cookies);
    QString storagePath() const;

};

} // namespace Mirall

#endif // MIRALL_COOKIEJAR_H
