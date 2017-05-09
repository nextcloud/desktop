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

#include <QLoggingCategory>
#include <QString>

#include "creds/credentialsfactory.h"
#include "creds/httpcredentialsgui.h"
#include "creds/dummycredentials.h"
#ifndef NO_SHIBBOLETH
#include "creds/shibbolethcredentials.h"
#endif

namespace OCC
{

Q_LOGGING_CATEGORY(lcGuiCredentials, "gui.credentials", QtInfoMsg)

namespace CredentialsFactory
{

AbstractCredentials* create(const QString& type)
{
    // empty string might happen for old version of configuration
    if (type == "http" || type == "") {
        return new HttpCredentialsGui;
    } else if (type == "dummy") {
        return new DummyCredentials;
#ifndef NO_SHIBBOLETH
    } else if (type == "shibboleth") {
        return new ShibbolethCredentials;
#endif
    } else {
        qCWarning(lcGuiCredentials, "Unknown credentials type: %s", qPrintable(type));
        return new DummyCredentials;
    }
}

} // ns CredentialsFactory

} // namespace OCC
