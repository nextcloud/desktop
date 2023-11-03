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
#include "common/filesystembase.h"
#include "common/version.h"
#include "plugin.h"
#include "syncjournaldb.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QPluginLoader>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif
using namespace OCC;

Q_LOGGING_CATEGORY(lcVfs, "sync.vfs", QtInfoMsg)

namespace {
QString modeToPluginName(Vfs::Mode mode)
{
    switch (mode) {
    case Vfs::Off:
        return QStringLiteral("off");
    case Vfs::WithSuffix:
        return QStringLiteral("suffix");
    case Vfs::WindowsCfApi:
        return QStringLiteral("win");
    default:
        Q_UNREACHABLE();
    }
}
}

Vfs::Vfs(QObject* parent)
    : QObject(parent)
{
}

QString Vfs::underlyingFileName(const QString &fileName) const
{
    return fileName;
}

Vfs::~Vfs() = default;

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

Result<void, QString> Vfs::checkAvailability(const QString &path, Vfs::Mode mode)
{
#ifdef Q_OS_WIN
    if (mode == Mode::WindowsCfApi) {
        const auto info = QFileInfo(path);
        if (QDir(info.canonicalFilePath()).isRoot()) {
            return tr("The Virtual filesystem feature does not support a drive as sync root");
        }
        const auto fs = FileSystem::fileSystemForPath(info.absoluteFilePath());
        if (fs != QLatin1String("NTFS")) {
            return tr("The Virtual filesystem feature requires a NTFS file system, %1 is using %2").arg(path, fs);
        }
        const auto type = GetDriveTypeW(reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(info.absoluteFilePath().mid(0, 3)).utf16()));
        if (type == DRIVE_REMOTE) {
            return tr("The Virtual filesystem feature is not supported on network drives");
        }
    }
#else
    Q_UNUSED(mode);
    Q_UNUSED(path);
#endif
    return {};
}

void Vfs::start(const VfsSetupParams &params)
{
    _setupParams = std::make_unique<VfsSetupParams>(params);
    startImpl(this->params());
}

bool Vfs::setPinStateInDb(const QString &folderPath, PinState state)
{
    auto path = folderPath.toUtf8();
    params().journal->internalPinStates().wipeForPathAndBelow(path);
    if (state != PinState::Inherited)
        params().journal->internalPinStates().setForPath(path, state);
    return true;
}

Optional<PinState> Vfs::pinStateInDb(const QString &folderPath)
{
    auto pin = params().journal->internalPinStates().effectiveForPath(folderPath.toUtf8());
    return pin;
}

Vfs::AvailabilityResult Vfs::availabilityInDb(const QString &folderPath)
{
    auto path = folderPath.toUtf8();
    auto pin = params().journal->internalPinStates().effectiveForPathRecursive(path);
    // not being able to retrieve the pin state isn't too bad
    auto hydrationStatus = params().journal->hasHydratedOrDehydratedFiles(path);
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

void Vfs::wipeDehydratedVirtualFiles()
{
    if (mode() == Vfs::Mode::Off) {
        // there are no placeholders
        return;
    }
    _setupParams->journal->getFilesBelowPath(QByteArray(), [&](const SyncJournalFileRecord &rec) {
        // only handle dehydrated files
        if (rec._type != ItemTypeVirtualFile && rec._type != ItemTypeVirtualFileDownload) {
            return;
        }
        const QString relativePath = QString::fromUtf8(rec._path);
        qCDebug(lcVfs) << "Removing db record for dehydrated file" << relativePath;
        _setupParams->journal->deleteFileRecord(relativePath);

        // If the local file is a dehydrated placeholder, wipe it too.
        // Otherwise leave it to allow the next sync to have a new-new conflict.
        const QString absolutePath = _setupParams->filesystemPath + relativePath;
        if (QFile::exists(absolutePath)) {
            // according to our db this is a dehydrated file, check it  to be sure
            if (isDehydratedPlaceholder(absolutePath)) {
                qCDebug(lcVfs) << "Removing local dehydrated placeholder" << relativePath;
                FileSystem::remove(absolutePath);
            }
        }
    });

    _setupParams->journal->forceRemoteDiscoveryNextSync();

    // Postcondition: No ItemTypeVirtualFile / ItemTypeVirtualFileDownload left in the db.
    // But hydrated placeholders may still be around.
}

Q_LOGGING_CATEGORY(lcPlugin, "plugins", QtInfoMsg)

OCC::VfsPluginManager *OCC::VfsPluginManager::_instance = nullptr;

bool OCC::VfsPluginManager::isVfsPluginAvailable(Vfs::Mode mode) const
{
    {
        auto result = _pluginCache.constFind(mode);
        if (result != _pluginCache.cend()) {
            return *result;
        }
    }
    const bool out = [mode] {
        const QString name = modeToPluginName(mode);
        if (!OC_ENSURE_NOT(name.isEmpty())) {
            return false;
        }
        auto pluginPath = pluginFileName(QStringLiteral("vfs"), name);
        QPluginLoader loader(pluginPath);

        auto basemeta = loader.metaData();
        if (basemeta.isEmpty() || !basemeta.contains(QStringLiteral("IID"))) {
            qCDebug(lcPlugin) << "Plugin doesn't exist:" << loader.fileName() << "LibraryPath:" << QCoreApplication::libraryPaths();
            return false;
        }
        if (basemeta[QStringLiteral("IID")].toString() != QLatin1String("org.owncloud.PluginFactory")) {
            qCWarning(lcPlugin) << "Plugin has wrong IID" << loader.fileName() << basemeta[QStringLiteral("IID")];
            return false;
        }

        auto metadata = basemeta[QStringLiteral("MetaData")].toObject();
        if (metadata[QStringLiteral("type")].toString() != QLatin1String("vfs")) {
            qCWarning(lcPlugin) << "Plugin has wrong type" << loader.fileName() << metadata[QStringLiteral("type")];
            return false;
        }
        if (metadata[QStringLiteral("version")].toString() != OCC::Version::version().toString()) {
            qCWarning(lcPlugin) << "Plugin has wrong version" << loader.fileName() << metadata[QStringLiteral("version")];
            return false;
        }

        // Attempting to load the plugin is essential as it could have dependencies that
        // can't be resolved and thus not be available after all.
        if (!loader.load()) {
            qCWarning(lcPlugin) << "Plugin failed to load:" << loader.errorString();
            return false;
        }

        return true;
    }();
    _pluginCache[mode] = out;
    return out;
}

Vfs::Mode OCC::VfsPluginManager::bestAvailableVfsMode() const
{
    if (isVfsPluginAvailable(Vfs::WindowsCfApi)) {
        return Vfs::WindowsCfApi;
    } else if (isVfsPluginAvailable(Vfs::WithSuffix)) {
        return Vfs::WithSuffix;
    } else if (isVfsPluginAvailable(Vfs::Off)) {
        return Vfs::Off;
    }
    Q_UNREACHABLE();
}

std::unique_ptr<Vfs> OCC::VfsPluginManager::createVfsFromPlugin(Vfs::Mode mode) const
{
    auto name = modeToPluginName(mode);
    if (name.isEmpty())
        return nullptr;
    auto pluginPath = pluginFileName(QStringLiteral("vfs"), name);

    if (!isVfsPluginAvailable(mode)) {
        qCCritical(lcPlugin) << "Could not load plugin: not existant or bad metadata" << pluginPath;
        return nullptr;
    }

    QPluginLoader loader(pluginPath);
    auto plugin = loader.instance();
    if (!plugin) {
        qCCritical(lcPlugin) << "Could not load plugin" << pluginPath << loader.errorString();
        return nullptr;
    }

    auto factory = qobject_cast<PluginFactory *>(plugin);
    if (!factory) {
        qCCritical(lcPlugin) << "Plugin" << loader.fileName() << "does not implement PluginFactory";
        return nullptr;
    }

    auto vfs = std::unique_ptr<Vfs>(qobject_cast<Vfs *>(factory->create(nullptr)));
    if (!vfs) {
        qCCritical(lcPlugin) << "Plugin" << loader.fileName() << "does not create a Vfs instance";
        return nullptr;
    }

    qCInfo(lcPlugin) << "Created VFS instance from plugin" << pluginPath;
    return vfs;
}

const VfsPluginManager &VfsPluginManager::instance()
{
    if (!_instance) {
        _instance = new VfsPluginManager();
    }
    return *_instance;
}

template <>
QString Utility::enumToString(Vfs::Mode mode)
{
    // Note: Strings are used for config and must be stable
    switch (mode) {
    case Vfs::Mode::WithSuffix:
        return QStringLiteral("suffix");
    case Vfs::Mode::WindowsCfApi:
        return QStringLiteral("wincfapi");
    case Vfs::Mode::Off:
        return QStringLiteral("off");
    }
    Q_UNREACHABLE();
}

VfsSetupParams::VfsSetupParams(const AccountPtr &account, const QUrl &baseUrl, bool groupInSidebar, SyncEngine *syncEngine)
    : account(account)
    , _baseUrl(baseUrl)
    , _groupInSidebar(groupInSidebar)
    , _syncEngine(syncEngine)
{
}

SyncEngine *VfsSetupParams::syncEngine() const
{
    return _syncEngine;
}
