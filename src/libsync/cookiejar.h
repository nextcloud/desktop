/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
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
    explicit CookieJar(QObject *parent = nullptr);
    ~CookieJar() override;
    bool setCookiesFromUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url) override;
    [[nodiscard]] QList<QNetworkCookie> cookiesForUrl(const QUrl &url) const override;

    void clearSessionCookies();

    using QNetworkCookieJar::setAllCookies;
    using QNetworkCookieJar::allCookies;

    bool save(const QString &fileName);
    bool restore(const QString &fileName);

signals:
    void newCookiesForUrl(const QList<QNetworkCookie> &cookieList, const QUrl &url);

private:
    QList<QNetworkCookie> removeExpired(const QList<QNetworkCookie> &cookies);
};

} // namespace OCC

#endif // MIRALL_COOKIEJAR_H
