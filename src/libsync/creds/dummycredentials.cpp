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
    return QStringLiteral("dummy");
}

QString DummyCredentials::user() const
{
    return _user;
}

AccessManager *DummyCredentials::createAM() const
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
    Q_EMIT(fetched());
}

void DummyCredentials::askFromUser()
{
    Q_EMIT(asked());
}

void DummyCredentials::persist()
{
}

} // namespace OCC
