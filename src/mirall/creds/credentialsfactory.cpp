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

#include <QString>

#include "mirall/creds/httpcredentials.h"
#include "mirall/creds/dummycredentials.h"
#include "mirall/creds/shibbolethcredentials.h"

namespace Mirall
{

namespace CredentialsFactory
{

AbstractCredentials* create(const QString& type)
{
    // empty string might happen for old version of configuration
    if (type == "http" || type == "") {
        return new HttpCredentials;
    } else if (type == "dummy") {
        return new DummyCredentials;
    } else if (type == "shibboleth") {
        return new ShibbolethCredentials;
    } else {
        qWarning("Unknown credentials type: %d", qPrintable(type));
        return new DummyCredentials;
    }
}

} // ns CredentialsFactory

} // ns Mirall
