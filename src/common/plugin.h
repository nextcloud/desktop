/*
 * Copyright (C) by Dominik Schmidt <dschmidt@owncloud.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include "ocsynclib.h"
#include <QObject>
#include <QPluginLoader>

namespace OCC {

class OCSYNC_EXPORT PluginFactory
{
public:
    ~PluginFactory();
    virtual QObject* create(QObject* parent) = 0;
};

template<class PLUGIN_CLASS>
class DefaultPluginFactory : public PluginFactory
{
public:
    QObject* create(QObject *parent) override
    {
        return new PLUGIN_CLASS(parent);
    }
};

class OCSYNC_EXPORT PluginLoader
{
public:
    static QString pluginName(const QString &type, const QString &name);

    bool isAvailable(const QString &type, const QString &name);
    QObject *load(const QString &type, const QString &name);
    QObject *create(const QString &type, const QString &name, QObject *parent = nullptr);
};

}

Q_DECLARE_INTERFACE(OCC::PluginFactory, "org.owncloud.PluginFactory")
