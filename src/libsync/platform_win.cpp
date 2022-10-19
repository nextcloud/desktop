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

    void setApplication(QCoreApplication *application) override;
};

WinPlatform::~WinPlatform()
{
}

std::unique_ptr<Platform> Platform::create()
{
    return std::make_unique<WinPlatform>();
}

void Platform::setApplication(QCoreApplication *application)
{
    Q_UNUSED(application)

    // Ensure OpenSSL config file is only loaded from app directory
    const QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
}

} // namespace OCC
