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

#include "cookiejar.h"

#include "configfile.h"

#include <QFile>
#include <QDateTime>
#include <QLoggingCategory>
#include <QNetworkCookie>
#include <QDataStream>

namespace OCC {

Q_LOGGING_CATEGORY(lcCookieJar, "sync.cookiejar", QtInfoMsg)

namespace {
    const unsigned int JAR_VERSION = 23;
}

QDataStream &operator<<(QDataStream &stream, const QList<QNetworkCookie> &list)
{
    stream << JAR_VERSION;
    stream << quint32(list.size());
    for (int i = 0; i < list.size(); ++i)
        stream << list.at(i).toRawForm();
    return stream;
}

QDataStream &operator>>(QDataStream &stream, QList<QNetworkCookie> &list)
{
    list.clear();

    quint32 version;
    stream >> version;

    if (version != JAR_VERSION)
        return stream;

    quint32 count;
    stream >> count;
    for (quint32 i = 0; i < count; ++i) {
        QByteArray value;
        stream >> value;
        QList<QNetworkCookie> newCookies = QNetworkCookie::parseCookies(value);
        if (newCookies.count() == 0 && value.length() != 0) {
            qCWarning(lcCookieJar) << "CookieJar: Unable to parse saved cookie:" << value;
        }
        for (int j = 0; j < newCookies.count(); ++j)
            list.append(newCookies.at(j));
        if (stream.atEnd())
            break;
    }
    return stream;
}

CookieJar::CookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
{
}

CookieJar::~CookieJar()
{
}

bool CookieJar::setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url)
{
    if (QNetworkCookieJar::setCookiesFromUrl(cookieList, url)) {
        Q_EMIT newCookiesForUrl(cookieList, url);
        return true;
    }

    return false;
}

QList<QNetworkCookie> CookieJar::cookiesForUrl(const QUrl &url) const
{
    QList<QNetworkCookie> cookies = QNetworkCookieJar::cookiesForUrl(url);
    qCDebug(lcCookieJar) << url << "requests:" << cookies;
    return cookies;
}

void CookieJar::clearSessionCookies()
{
    setAllCookies(removeExpired(allCookies()));
}

void CookieJar::save(const QString &fileName)
{
    QFile file;
    file.setFileName(fileName);
    qCDebug(lcCookieJar) << fileName;
    file.open(QIODevice::WriteOnly);
    QDataStream stream(&file);
    stream << removeExpired(allCookies());
    file.close();
}

void CookieJar::restore(const QString &fileName)
{
    QFile file;
    file.setFileName(fileName);
    file.open(QIODevice::ReadOnly);
    QDataStream stream(&file);
    QList<QNetworkCookie> list;
    stream >> list;
    setAllCookies(removeExpired(list));
    file.close();
}

QList<QNetworkCookie> CookieJar::removeExpired(const QList<QNetworkCookie> &cookies)
{
    QList<QNetworkCookie> updatedList;
    foreach (const QNetworkCookie &cookie, cookies) {
        if (cookie.expirationDate() > QDateTime::currentDateTimeUtc() && !cookie.isSessionCookie()) {
            updatedList << cookie;
        }
    }
    return updatedList;
}

} // namespace OCC
