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

#include <QDebug>
#include <QTextStream>

#include "creds/shibboleth/shibbolethconfigfile.h"
#include "creds/shibboleth/shibbolethcookiejar.h"

namespace Mirall
{

namespace
{

const char otherCookiesC[] = "otherCookies";

} // ns

void ShibbolethConfigFile::storeCookies(const QMap<QUrl, QList<QNetworkCookie> >& cookiesForUrl)
{
    if (cookiesForUrl.isEmpty()) {
        removeData(QString(), QString::fromLatin1(otherCookiesC));
    } else {
        QByteArray data;
        QTextStream stream(&data);

        Q_FOREACH (const QUrl& url, cookiesForUrl.keys()) {
            const QList<QNetworkCookie>& cookies(cookiesForUrl[url]);

            if (cookies.isEmpty()) {
                continue;
            }
            stream << "URL: " << url.toString().toUtf8() << "\n";
            qDebug() << "URL: " << url.toString().toUtf8();

            Q_FOREACH (const QNetworkCookie& cookie, cookies) {
                stream << cookie.toRawForm(QNetworkCookie::NameAndValueOnly) << "\n";
                qDebug() << cookie.toRawForm(QNetworkCookie::NameAndValueOnly);
            }
        }

        stream.flush();

        const QByteArray encodedCookies(data.toBase64());

        qDebug() << "Raw cookies:\n" << data;
        qDebug() << "Encoded cookies: " << encodedCookies;

        storeData(QString(), QString::fromLatin1(otherCookiesC), QVariant(encodedCookies));
    }
}

ShibbolethCookieJar* ShibbolethConfigFile::createCookieJar() const
{
    ShibbolethCookieJar* jar = new ShibbolethCookieJar();
    const QVariant variant(retrieveData(QString(), QString::fromLatin1(otherCookiesC)));

    if (variant.isValid()) {
        QByteArray data(QByteArray::fromBase64(variant.toByteArray()));
        QTextStream stream (&data);
        const QString urlHeader(QString::fromLatin1("URL: "));
        QUrl currentUrl;
        QList<QNetworkCookie> currentCookies;

        qDebug() << "Got valid cookies variant: " << data;

        while (!stream.atEnd()) {
            const QString line(stream.readLine());

            qDebug() << line;

            if (line.startsWith(urlHeader)) {
                if (!currentUrl.isEmpty() && !currentCookies.isEmpty()) {
                    jar->setCookiesFromUrl(currentCookies, currentUrl);
                    currentCookies.clear();
                    currentUrl.clear();
                }
                currentUrl = QUrl(line.mid(5));
            } else if (!currentUrl.isEmpty()) {
                const int equalPos(line.indexOf('='));

                currentCookies << QNetworkCookie(line.left(equalPos).toUtf8(), line.mid(equalPos + 1).toUtf8());
            }
        }
        if (!currentUrl.isEmpty() && !currentCookies.isEmpty()) {
            jar->setCookiesFromUrl(currentCookies, currentUrl);
        }
    }

    return jar;
}

} // ns Mirall
