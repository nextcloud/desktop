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

#include "vfs.h"
#include "plugin.h"

using namespace OCC;

Vfs::Vfs(QObject* parent)
    : QObject(parent)
{
}

Vfs::~Vfs() = default;

QString Vfs::modeToString(Mode mode)
{
    // Note: Strings are used for config and must be stable
    switch (mode) {
    case Off:
        return QStringLiteral("off");
    case WithSuffix:
        return QStringLiteral("suffix");
    case WindowsCfApi:
        return QStringLiteral("wincfapi");
    }
    return QStringLiteral("off");
}

bool Vfs::modeFromString(const QString &str, Mode *mode)
{
    // Note: Strings are used for config and must be stable
    *mode = Off;
    if (str == "off") {
        return true;
    } else if (str == "suffix") {
        *mode = WithSuffix;
        return true;
    } else if (str == "wincfapi") {
        *mode = WindowsCfApi;
        return true;
    }
    return false;
}

static QString modeToPluginName(Vfs::Mode mode)
{
    if (mode == Vfs::WithSuffix)
        return "suffix";
    if (mode == Vfs::WindowsCfApi)
        return "win";
    return QString();
}

bool OCC::isVfsPluginAvailable(Vfs::Mode mode)
{
    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return false;
    return PluginLoader().load("vfs", name);
}

Vfs::Mode OCC::bestAvailableVfsMode()
{
    if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
        return Vfs::WindowsCfApi;
    } else if (isVfsPluginAvailable(Vfs::WithSuffix)) {
        return Vfs::WithSuffix;
    }
    return Vfs::Off;
}

Vfs *OCC::createVfsFromPlugin(Vfs::Mode mode, QObject *parent)
{
    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return nullptr;
    return qobject_cast<Vfs *>(PluginLoader().create("vfs", name, parent));
}
