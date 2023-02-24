/*
 * Copyright (C) by Erik Verbruggen <erik@verbruggen.consulting>
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

#include "application.h"
#include "guiutility.h"

#include <QCoreApplication>

namespace OCC {

void Utility::startShellIntegration()
{
}

QString Utility::socketApiSocketPath()
{
    return QLatin1String("\\\\.\\pipe\\") + QLatin1String("ownCloud-") + qEnvironmentVariable("USERNAME");
    // TODO: once the windows extension supports multiple
    // client connections, switch back to the theme name
    // See issue #2388
    // + Theme::instance()->appName();
}

} // namespace OCC
