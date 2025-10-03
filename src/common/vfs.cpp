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
#include "version.h"
#include "syncjournaldb.h"

#include "common/filesystembase.h"

#include <QPluginLoader>
#include <QLoggingCategory>

using namespace OCC;

using MetaObjectHash = QHash<QString, Vfs::Factory>;
Q_GLOBAL_STATIC(MetaObjectHash, vfsFactoryHash);

void Vfs::registerPlugin(const QString &name, Factory factory)
{
    Q_ASSERT(!vfsFactoryHash()->contains(name));
    vfsFactoryHash()->insert(name, factory);
}

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
    case XAttr:
        return QStringLiteral("xattr");
    }
    return QStringLiteral("off");
}

Optional<Vfs::Mode> Vfs::modeFromString(const QString &str)
{
    // Note: Strings are used for config and must be stable
    if (str == QLatin1String("off")) {
        return Off;
    } else if (str == QLatin1String("suffix")) {
        return WithSuffix;
    } else if (str == QLatin1String("wincfapi")) {
        return WindowsCfApi;
    }
    return {};
}

Result<bool, QString> Vfs::checkAvailability(const QString &path)
{
    const auto mode = bestAvailableVfsMode();
#ifdef Q_OS_WIN
    if (mode == Mode::WindowsCfApi) {
        const auto fs = FileSystem::fileSystemForPath(path);
        if (fs != QLatin1String("NTFS")) {
            return tr("The Virtual filesystem feature requires a NTFS file system, %1 is using %2").arg(path, fs);
        }
    }
#else
    Q_UNUSED(path);
#endif
    return true;
}

void Vfs::start(const VfsSetupParams &params)
{
    _setupParams = params;
    startImpl(params);
}

bool Vfs::setPinStateInDb(const QString &folderPath, PinState state)
{
    auto path = folderPath.toUtf8();
    _setupParams.journal->internalPinStates().wipeForPathAndBelow(path);
    if (state != PinState::Inherited)
        _setupParams.journal->internalPinStates().setForPath(path, state);
    return true;
}

Optional<PinState> Vfs::pinStateInDb(const QString &folderPath)
{
    auto pin = _setupParams.journal->internalPinStates().effectiveForPath(folderPath.toUtf8());
    return pin;
}

Vfs::AvailabilityResult Vfs::availabilityInDb(const QString &folderPath)
{
    auto path = folderPath.toUtf8();
    auto pin = _setupParams.journal->internalPinStates().effectiveForPathRecursive(path);
    // not being able to retrieve the pin state isn't too bad
    auto hydrationStatus = _setupParams.journal->hasHydratedOrDehydratedFiles(path);
    if (!hydrationStatus)
        return AvailabilityError::DbError;

    if (hydrationStatus->hasDehydrated) {
        if (hydrationStatus->hasHydrated)
            return VfsItemAvailability::Mixed;
        if (pin && *pin == PinState::OnlineOnly)
            return VfsItemAvailability::OnlineOnly;
        else
            return VfsItemAvailability::AllDehydrated;
    } else if (hydrationStatus->hasHydrated) {
        if (pin && *pin == PinState::AlwaysLocal)
            return VfsItemAvailability::AlwaysLocal;
        else
            return VfsItemAvailability::AllHydrated;
    }
    return AvailabilityError::NoSuchItem;
}

VfsOff::VfsOff(QObject *parent)
    : Vfs(parent)
{
}

VfsOff::~VfsOff() = default;

static QString modeToPluginName(Vfs::Mode mode)
{
    if (mode == Vfs::WithSuffix)
        return QStringLiteral("suffix");
    if (mode == Vfs::WindowsCfApi)
        return QStringLiteral("win");
    if (mode == Vfs::XAttr)
        return QStringLiteral("xattr");
    return QString();
}

Q_LOGGING_CATEGORY(lcPlugin, "plugins", QtInfoMsg)

bool OCC::isVfsPluginAvailable(Vfs::Mode mode)
{
    // TODO: cache plugins available?
    if (mode == Vfs::Off)
        return true;
    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return false;

    if (!vfsFactoryHash()->contains(name)) {
        qCDebug(lcPlugin) << "Plugin isn't registered:" << name;
        return false;
    }

    return true;
}

Vfs::Mode OCC::bestAvailableVfsMode()
{
    if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
        return Vfs::WindowsCfApi;
    }

    if (isVfsPluginAvailable(Vfs::WithSuffix)) {
        return Vfs::WithSuffix;
    }

    // For now the "suffix" backend has still precedence over the "xattr" backend.
    // Ultimately the order of those ifs will change when xattr will be more mature.
    // But what does "more mature" means here?
    //
    //  * On Mac when it properly reads and writes com.apple.LaunchServices.OpenWith
    // This will require reverse engineering to see what they stuff in there. Maybe a good
    // starting point:
    // https://eclecticlight.co/2017/12/20/xattr-com-apple-launchservices-openwith-sets-a-custom-app-to-open-a-file/
    //
    //  * On Linux when our user.nextcloud.hydrate_exec is adopted by at least KDE and Gnome
    // the "user.nextcloud" prefix might turn into "user.xdg" in the process since it would
    // be best to have a freedesktop.org spec for it.
    // When that time comes, it might still require detecting at runtime if that's indeed
    // supported in the user session or even per sync folder (in case user would pick a folder
    // which wouldn't support xattr for some reason)

    if (isVfsPluginAvailable(Vfs::XAttr)) {
        return Vfs::XAttr;
    }

    return Vfs::Off;
}

std::unique_ptr<Vfs> OCC::createVfsFromPlugin(Vfs::Mode mode)
{
    if (mode == Vfs::Off)
        return std::unique_ptr<Vfs>(new VfsOff);

    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return nullptr;

    if (!isVfsPluginAvailable(mode)) {
        qCCritical(lcPlugin) << "Could not load plugin: not existant" << name;
        return nullptr;
    }

    const auto factory = vfsFactoryHash()->value(name);
    if (!factory) {
        qCCritical(lcPlugin) << "Could not load plugin" << name;
        return nullptr;
    }

    auto vfs = std::unique_ptr<Vfs>(qobject_cast<Vfs *>(factory()));
    if (!vfs) {
        qCCritical(lcPlugin) << "Plugin" << name << "does not create a Vfs instance";
        return nullptr;
    }

    qCInfo(lcPlugin) << "Created VFS instance for:" << name;
    return vfs;
}
