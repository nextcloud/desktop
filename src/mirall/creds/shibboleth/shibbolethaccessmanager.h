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

#ifndef MIRALL_WIZARD_SHIBBOLETH_ACCESS_MANAGER_H
#define MIRALL_WIZARD_SHIBBOLETH_ACCESS_MANAGER_H

#include <QNetworkAccessManager>
#include <QNetworkCookie>

namespace Mirall
{

class ShibbolethAccessManager : public QNetworkAccessManager
{
    Q_OBJECT

public:
    ShibbolethAccessManager (const QNetworkCookie& cookie, QObject* parent = 0);

protected:
    QNetworkReply* createRequest (QNetworkAccessManager::Operation op, const QNetworkRequest& request, QIODevice* outgoingData = 0);

private:
    QNetworkCookie _cookie;
};

} // ns Mirall

#endif
