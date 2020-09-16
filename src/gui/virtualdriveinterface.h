/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#pragma once

#include <QObject>

#include "common/syncjournaldb.h"

namespace OCC {

class VirtualDriveInterface : public QObject
{
    Q_OBJECT
public:
    explicit VirtualDriveInterface(QObject *parent = nullptr);
    ~VirtualDriveInterface();

public slots:
    virtual void mount() = 0;
    virtual void unmount() = 0;
};
}
