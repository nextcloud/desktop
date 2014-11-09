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

#include "creds/credentialsfactory.h"
#ifdef TOKEN_AUTH_ONLY
#include "creds/tokencredentials.h"
#else
#include "creds/httpcredentials.h"
#include "creds/dummycredentials.h"
#include "creds/shibbolethcredentials.h"
#endif

namespace OCC
{

namespace CredentialsFactory
{

AbstractCredentials* create(const QString& type)
{
#ifdef TOKEN_AUTH_ONLY
	return new TokenCredentials;
#else
	
    // empty string might happen for old version of configuration
    if (type == "http" || type == "") {
        return new HttpCredentialsGui;
    } else if (type == "dummy") {
        return new DummyCredentials;
    } else if (type == "shibboleth") {
        return new ShibbolethCredentials;
    } else {
        qWarning("Unknown credentials type: %s", qPrintable(type));
        return new DummyCredentials;
    }
#endif
}

} // ns CredentialsFactory

} // namespace OCC
