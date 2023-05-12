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

#ifndef PLATFORM_H
#define PLATFORM_H

#include <QCoreApplication>
#include <QString>

#include <memory>

#include "owncloudlib.h"

namespace OCC {

/**
 * @brief The Platform is the baseclass for all platform classes, which in turn implement platform
 *        specific functionality for the GUI.
 */
class OWNCLOUDSYNC_EXPORT Platform : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    static std::unique_ptr<Platform> create();

    virtual void migrate();

    virtual void setApplication(QCoreApplication *application);

    virtual void startServices();

Q_SIGNALS:
    void requestAttention();
};

} // OCC namespace

#endif // PLATFORM_H
