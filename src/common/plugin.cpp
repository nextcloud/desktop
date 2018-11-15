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

#include "plugin.h"

#include "config.h"

#include <QPluginLoader>
#include <QDir>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcPluginLoader, "pluginLoader", QtInfoMsg)

namespace OCC {

PluginFactory::~PluginFactory() = default;

QString PluginLoader::pluginName(const QString &type, const QString &name)
{
    return QString(QLatin1String("%1sync_%2_%3"))
            .arg(APPLICATION_EXECUTABLE, type, name);
}

bool PluginLoader::isAvailable(const QString &type, const QString &name)
{
    QPluginLoader loader(pluginName(type, name));
    return loader.load();
}

QObject *PluginLoader::load(const QString &type, const QString &name)
{
    QString fileName = pluginName(type, name);
    QPluginLoader pluginLoader(fileName);
    if (pluginLoader.load()) {
        qCInfo(lcPluginLoader) << "Loaded plugin" << fileName;
    } else {
        qCWarning(lcPluginLoader) << "Could not load plugin"
                                  << fileName <<":"
                                  << pluginLoader.errorString()
                                  << "from" << QDir::currentPath();
    }

    return pluginLoader.instance();
}

QObject *PluginLoader::create(const QString &type, const QString &name, QObject *parent)
{
    auto factory = qobject_cast<PluginFactory *>(load(type, name));
    if (!factory) {
        return nullptr;
    } else {
        return factory->create(parent);
    }
}

}
