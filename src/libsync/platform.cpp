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

#include "platform.h"

#include "platform_unix.h"
#include "platform_win.h"

#if defined(Q_OS_MAC)
#include "platform_mac.h"
#endif

#include "configfile.h"

#ifdef CRASHREPORTER_EXECUTABLE
#include <QDir>

#include <libcrashreporter-handler/Handler.h>

#endif

namespace OCC {

void Platform::migrate()
{
}

void Platform::setApplication(QCoreApplication *application)
{
#ifdef CRASHREPORTER_EXECUTABLE
    if (ConfigFile().crashReporter()) {
        auto *crashHandler =
            new CrashReporter::Handler(QDir::tempPath(), true, QStringLiteral("%1/" CRASHREPORTER_EXECUTABLE).arg(application->applicationDirPath()));
        connect(application, &QCoreApplication::aboutToQuit, this, [crashHandler] { delete crashHandler; });
    }
#endif
}

void Platform::startServices() { }

std::unique_ptr<Platform> Platform::create()
{
    // we need to make sure the platform class is initialized before a Q(Core)Application has been set up
    // the constructors run some initialization code that affects Qt's initialization
    Q_ASSERT(QCoreApplication::instance() == nullptr);

#if defined(Q_OS_WIN)
    return std::make_unique<WinPlatform>();
#elif defined(Q_OS_LINUX)
    return std::make_unique<UnixPlatform>();
#elif defined(Q_OS_MAC)
    return std::make_unique<MacPlatform>();
#else
    Q_UNREACHABLE();
#endif
}

} // OCC namespace
