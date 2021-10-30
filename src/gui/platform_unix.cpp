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

#include <QLoggingCategory>

#include <signal.h>
#include <sys/resource.h>
#include <sys/time.h>

#include "guiutility.h"
#include "platform.h"

namespace OCC {

Q_LOGGING_CATEGORY(lcPlatform, "gui.platform")

class UnixPlatform : public Platform
{
public:
    UnixPlatform()
    {
        signal(SIGPIPE, SIG_IGN);
        setLimitsForCoreDumps();
    }

    ~UnixPlatform() override;

private:
    void setLimitsForCoreDumps();
};

UnixPlatform::~UnixPlatform()
{
}

void UnixPlatform::setLimitsForCoreDumps()
{
    // check a environment variable for core dumps
    if (!qEnvironmentVariableIsEmpty("OWNCLOUD_CORE_DUMP")) {
        struct rlimit core_limit;
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
            fprintf(stderr, "Unable to set core dump limit\n");
        } else {
            qCInfo(lcPlatform) << "Core dumps enabled";
        }
    }
}

std::unique_ptr<Platform> Platform::create()
{
    return std::make_unique<UnixPlatform>();
}

} // namespace OCC
