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
#include <QDir>

namespace OCC {

Q_LOGGING_CATEGORY(lcCookieJar, "sync.cookiejar", QtInfoMsg)

CookieJar::CookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
{
}

CookieJar::~CookieJar()
{
}

} // namespace OCC
