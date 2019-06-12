/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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
#include <QScopedPointer>

namespace OCC {

struct LibCloudProvidersPrivate;

/** Exports sync folders to dbus with libcloudproviders */
struct LibCloudProviders : public QObject
{
    Q_OBJECT

    QScopedPointer<LibCloudProvidersPrivate> d_ptr;
public:
    explicit LibCloudProviders(QObject *parent = nullptr);

    void start();

signals:
    void showSettings();
};

} // namespace OCC
