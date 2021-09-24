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

#include "creds/dummycredentials.h"
#include "accessmanager.h"

namespace OCC {

QString DummyCredentials::authType() const
{
    return QString::fromLatin1("dummy");
}

QString DummyCredentials::user() const
{
    return _user;
}

QString DummyCredentials::password() const
{
    Q_UNREACHABLE();
    return QString();
}

QNetworkAccessManager *DummyCredentials::createQNAM() const
{
    return new AccessManager;
}

bool DummyCredentials::ready() const
{
    return true;
}

bool DummyCredentials::stillValid(QNetworkReply *reply)
{
    Q_UNUSED(reply)
    return true;
}

void DummyCredentials::fetchFromKeychain()
{
    _wasFetched = true;
    emit fetched();
}

void DummyCredentials::askFromUser()
{
    emit asked();
}

void DummyCredentials::persist()
{
}

} // namespace OCC
