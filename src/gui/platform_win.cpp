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
#include "platform.h"

#include <QCoreApplication>

namespace OCC {

class WinPlatform : public Platform
{
public:
    WinPlatform()
    {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    }

    ~WinPlatform() override;
};

WinPlatform::~WinPlatform()
{
}

std::unique_ptr<Platform> Platform::create(const QString &appDomain)
{
    Q_UNUSED(appDomain);
    return std::make_unique<WinPlatform>();
}

} // namespace OCC
