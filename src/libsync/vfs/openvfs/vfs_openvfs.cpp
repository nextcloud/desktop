/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2025 OpenCloud GmbH and OpenCloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "vfs_openvfs.h"

#include "account.h"
#include "common/syncjournaldb.h"
#include "filesystem.h"
#include "libsync/theme.h"
#include "xattr.h"
#include "syncfileitem.h"
#include "hydrationjob.h"
#include "common/path.h"

#include <openvfs/openvfs.h>

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QString>
#include <QUuid>
#include <QStandardPaths>

#include <filesystem>

using namespace std::chrono_literals;
using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(lcOpenVFS, "sync.vfs.openvfs", QtInfoMsg)


namespace {
OCC::FileSystem::Path openVFSExePath()
{
    auto openVFS = OCC::FileSystem::Path(std::filesystem::path(OPENVFS_EXE));
    auto binary = OCC::FileSystem::Path(qApp->applicationDirPath()) / openVFS.get().filename();
    if (binary.exists()) {
        return binary;
    }
    return openVFS;
}


QString xattrOwnerString(const QUuid &accountUuid)
{
    return u"%1:%2"_s.arg(OCC::Theme::instance()->appName(), accountUuid.toString(QUuid::WithoutBraces));
}

OCC::FileSystem::Path openVFSConfigFilePath()
{
    auto systemPath = OCC::FileSystem::Path(QStandardPaths::locate(QStandardPaths::ConfigLocation, u"openvfs/config.json"_s));
    if (!systemPath.get().empty()) {
        return systemPath;
    }
    // if (OCC::Utility::runningInAppImage()) {
    //     auto appimagePath = OCC::FileSystem::Path(qApp->applicationDirPath()) / "../etc/xdg/openvfs/config.json";
    //     if (appimagePath.exists()) {
    //         return appimagePath;
    //     }
    // }
    return {};
}

OpenVFS::PlaceHolderAttributes placeHolderAttributes(const std::filesystem::path &path)
{
    const auto data = OCC::FileSystem::Xattr::getxattr(path, QString::fromUtf8(OpenVFS::Constants::XAttributeNames::Data));
    if (!data) {
        qCWarning(lcOpenVFS) << u"No OpenVFS xattr found for" << path.native();
    }
    return OpenVFS::PlaceHolderAttributes::fromData(path, data ? std::vector<uint8_t>{data->cbegin(), data->cend()} : std::vector<uint8_t>{});
}

OpenVFS::PlaceHolderAttributes placeHolderAttributes(const QString &path)
{
    return placeHolderAttributes(OCC::FileSystem::toFilesystemPath(path));
}

OCC::Result<void, QString> setPlaceholderAttributes(const OpenVFS::PlaceHolderAttributes &attributes)
{
    Q_ASSERT(attributes.validate());
    const auto data = attributes.toData();
    return OCC::FileSystem::Xattr::setxattr(attributes.absolutePath, QString::fromUtf8(OpenVFS::Constants::XAttributeNames::Data),
        {reinterpret_cast<const char *>(data.data()), static_cast<qsizetype>(data.size())});
}

OCC::Result<void, QString> setPlaceholderAttributes(const OpenVFS::PlaceHolderAttributes &attributes, time_t modtime)
{
    if (const auto result = setPlaceholderAttributes(attributes); !result) {
        return result;
    }
    OCC::FileSystem::setModTime(attributes.absolutePath, modtime);
    return {};
}

OpenVFS::Constants::PinStates convertPinState(OCC::PinState pState)
{
    switch (pState) {
    case OCC::PinState::AlwaysLocal:
        return OpenVFS::Constants::PinStates::AlwaysLocal;
    case OCC::PinState::Inherited:
        return OpenVFS::Constants::PinStates::Inherited;
    case OCC::PinState::OnlineOnly:
        return OpenVFS::Constants::PinStates::OnlineOnly;
    case OCC::PinState::Excluded:
        return OpenVFS::Constants::PinStates::Excluded;
    case OCC::PinState::Unspecified:
        return OpenVFS::Constants::PinStates::Unspecified;
    };
    Q_UNREACHABLE();
}

OCC::PinState convertPinState(OpenVFS::Constants::PinStates pState)
{
    switch (pState) {
    case OpenVFS::Constants::PinStates::AlwaysLocal:
        return OCC::PinState::AlwaysLocal;
    case OpenVFS::Constants::PinStates::Inherited:
        return OCC::PinState::Inherited;
    case OpenVFS::Constants::PinStates::OnlineOnly:
        return OCC::PinState::OnlineOnly;
    case OpenVFS::Constants::PinStates::Unspecified:
        return OCC::PinState::Unspecified;
    case OpenVFS::Constants::PinStates::Excluded:
        return OCC::PinState::Excluded;
    }
    Q_UNREACHABLE();
}

#ifdef Q_OS_LINUX

// Helper function to parse paths that the kernel inserts escape sequences
// for.
// https://github.com/qt/qtbase/blob/f47d9bcb45c77183c23e406df415ec2d9f4acbc4/src/corelib/io/qstorageinfo_linux.cpp#L72
QByteArray parseMangledPath(QByteArrayView path)
{
    // The kernel escapes with octal the following characters:
    //  space ' ', tab '\t', backslash '\\', and newline '\n'
    // See:
    // https://codebrowser.dev/linux/linux/fs/proc_namespace.c.html#show_mountinfo
    // https://codebrowser.dev/linux/linux/fs/seq_file.c.html#mangle_path

    QByteArray ret(path.size(), '\0');
    char *dst = ret.data();
    const char *src = path.data();
    const char *srcEnd = path.data() + path.size();
    while (src != srcEnd) {
        switch (*src) {
        case ' ': // Shouldn't happen
            return {};

        case '\\': {
            // It always uses exactly three octal characters.
            ++src;
            char c = (*src++ - '0') << 6;
            c |= (*src++ - '0') << 3;
            c |= (*src++ - '0');
            *dst++ = c;
            break;
        }

        default:
            *dst++ = *src++;
            break;
        }
    }
    // If "path" contains any of the characters this method is demangling,
    // "ret" would be oversized with extra '\0' characters at the end.
    ret.resize(dst - ret.data());
    return ret;
}
#endif
}

namespace OCC {

OpenVFS::OpenVFS(QObject *parent)
    : Vfs(parent)
{
}

OpenVFS::~OpenVFS() = default;

Vfs::Mode OpenVFS::mode() const
{
    return Mode::OpenVFS;
}

void OpenVFS::startImpl(const VfsSetupParams &params)
{
    qCDebug(lcOpenVFS, "Start OpenVFS VFS");

    // Lets claim the sync root directory for us
    // set the owner to opencloud to claim it
    const auto owner = xattrOwnerString(params.account->uuid()).toStdString();
    if (const auto info = ::OpenVFS::Registration::registerFilesystem(std::filesystem::path{params.filesystemPath.toStdWString()}, owner); !info) {
        if (info.owner() != owner) {
            Q_EMIT error(tr("Unable to claim the sync root for files on demand, the folder is already claimed by %1").arg(info.owner()));
            return;
        }
        Q_EMIT error(tr("Unable to retrieve registration info. Error: %1").arg(info.error()));
        return;
    }

    qCDebug(lcOpenVFS) << "Mounting" << openVFSExePath().toString() << params.filesystemPath;
    _openVfsProcess = new QProcess(this);
    // merging the channels and piping the output to our log lead to deadlocks
    _openVfsProcess->setProcessChannelMode(QProcess::ForwardedChannels);
    const auto logPrefix = [path = params.filesystemPath, this] { return u"[%1 %2] "_s.arg(QString::number(_openVfsProcess->processId()), path); };
    connect(_openVfsProcess, &QProcess::finished, this, [logPrefix, this] { qCFatal(lcOpenVFS) << logPrefix() << "finished" << _openVfsProcess->exitCode(); });
    connect(_openVfsProcess, &QProcess::started, this, [logPrefix, this] {
        qCInfo(lcOpenVFS) << logPrefix() << u"started";
        // TODO:
        // give it time to mount
        QTimer::singleShot(1s, this, &Vfs::started);
    });
    connect(_openVfsProcess, &QProcess::errorOccurred, this, [logPrefix, this] { qCWarning(lcOpenVFS) << logPrefix() << _openVfsProcess->errorString(); });

    QStringList pparams{u"-f"_s, u"-i"_s, openVFSConfigFilePath().toString(), u"-o"_s, xattrOwnerString(params.account->uuid()), u"-s"_s, params.socketPath,
        params.filesystemPath};

    // Enable _lots_ of logging for fuse layer
    if (qEnvironmentVariableIntValue("OPENCLOUD_OPENVFS_LOG_FUSE") == 1) {
        pparams.prepend(u"-d"_s);
    }

    qCDebug(lcOpenVFS) << "Starting openvfs" << openVFSExePath().toString() << pparams;
    _openVfsProcess->start(openVFSExePath().toString(), pparams, QIODevice::ReadOnly);
}

void OpenVFS::stop()
{
    if (_openVfsProcess) {
        // disconnect qFatal on subprocess exit
        disconnect(_openVfsProcess, &QProcess::finished, nullptr, nullptr);
        _openVfsProcess->terminate();
        _openVfsProcess->waitForFinished();
        _openVfsProcess->deleteLater();
    }
}

void OpenVFS::unregisterFolder()
{
    ::OpenVFS::Registration::unregisterFilesystem({std::filesystem::path{params().filesystemPath.toStdWString()}, xattrOwnerString(params().account->uuid()).toStdString()});
}

bool OpenVFS::socketApiPinStateActionsShown() const
{
    return true;
}


bool OpenVfsPluginFactory::checkAvailability() const
{
#ifdef Q_OS_LINUX
    if (!FileSystem::Path("/dev/fuse").exists()) {
        qCWarning(lcOpenVFS) << u"Fuse is not installed or available on the system";
        return false;
    }
    if (QStandardPaths::findExecutable(u"fusermount3"_s).isEmpty()) {
        qCWarning(lcOpenVFS) << u"fusermount3 is not installed on the system";
        return false;
    }
#endif
    return true;
}

Result<void, QString> OpenVfsPluginFactory::prepare(const QString &path, const QUuid &accountUuid) const
{
#ifdef Q_OS_LINUX
    // we can't use QStorageInfo as it does not list fuse mounts
    if (!_cacheTimer.isStarted() || _cacheTimer.duration() > 30s) {
        _fuseMountCache.clear();
        QFile file(u"/proc/self/mountinfo"_s);
        if (file.open(QIODevice::ReadOnly)) {
            const auto lines = file.readAll().split('\n');
            file.close();
            for (auto &line : lines) {
                auto fields = line.split(' ');
                if (fields.size() >= 9 && fields[8] == "fuse.openvfs") {
                    _fuseMountCache << QString::fromUtf8(parseMangledPath(fields[4]));
                }
            }
        } else {
            qCWarning(lcOpenVFS) << "Failed to read /proc/self/mountinfo" << file.errorString();
            return tr("Failed to read /proc/self/mountinfo");
        }
    }
    if (std::ranges::find_if(_fuseMountCache, [&](const QString &p) { return FileSystem::isChildPathOf2(path, p).testFlag(FileSystem::ChildResult::IsEqual); })
        != _fuseMountCache.cend()) {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(u"fusermount"_s, {u"-zu"_s, path});
        // TODO: don't block?
        process.waitForFinished();
        if (process.exitCode() != 0) {
            const auto output = process.readAll();
            qCWarning(lcOpenVFS) << "Failed to unmount the OpenVFS mount" << path << output;
            return tr("Failed to unmount the OpenVFS mount %1 Error:%2").arg(path, output);
        } else {
            qCDebug(lcOpenVFS) << "Unmounted OpenVFS mount" << path;
        }
    }
#endif
    const auto fsPath = FileSystem::toFilesystemPath(path);
    if (!FileSystem::Xattr::supportsxattr(fsPath)) {
        qCDebug(lcOpenVFS) << path << "does not support xattributes";
        return tr("The filesystem for %1 does not support xattributes.").arg(path);
    }
    if (const auto info = ::OpenVFS::Registration::fromAttributes(fsPath); info && info.owner() != xattrOwnerString(accountUuid).toStdString()) {
        return tr("The sync path is already claimed by %1").arg(info.owner());
    }
    if (!openVFSExePath().exists()) {
        qCDebug(lcOpenVFS) << "OpenVFS executable not found at" << openVFSExePath().toString();
        return tr("OpenVFS executable not found, please install it");
    }
    const auto vfsConfig = openVFSConfigFilePath();
    if (!vfsConfig.get().empty()) {
        qCDebug(lcOpenVFS) << "Using config file" << vfsConfig.toString();
    } else {
        return tr("Failed to find the OpenVFS config file, please check your installation.");
    }
    return {};
}

OCC::Result<OCC::Vfs::ConvertToPlaceholderResult, QString> OpenVFS::updateMetadata(
    const SyncFileItem &syncItem, const QString &filePath, const QString &replacesFile)
{
    if (syncItem._type == ItemTypeVirtualFileDehydration) {
        // replace the file with a placeholder
        if (const auto result = createPlaceholder(syncItem); !result) {
            qCCritical(lcOpenVFS) << "Failed to create placeholder for" << filePath << result.error();
            return result.error();
        }
        return ConvertToPlaceholderResult::Ok;
    } else {
        ::OpenVFS::PlaceHolderAttributes attributes = [&] {
            // load the previous attributes
            if (!replacesFile.isEmpty()) {
                if (const auto attr = placeHolderAttributes(replacesFile)) {
                    return attr;
                }
            }
            if (const auto attr = placeHolderAttributes(filePath)) {
                return attr;
            }
            Q_ASSERT(QFileInfo::exists(filePath));
            // generate new meta data for an existing file
            auto attr = ::OpenVFS::PlaceHolderAttributes::create(
                FileSystem::toFilesystemPath(filePath), syncItem._etag.toStdString(), syncItem._fileId.toStdString(), syncItem._size);
            attr.state = ::OpenVFS::Constants::States::Hydrated;
            return attr;
        }();
        Q_ASSERT(attributes);

        attributes.size = syncItem._size;
        attributes.fileId = syncItem._fileId.toStdString();
        attributes.etag = syncItem._etag.toStdString();

        qCDebug(lcOpenVFS) << attributes.absolutePath.native() << syncItem._type;

        switch (syncItem._type) {
        case ItemTypeVirtualFileDownload:
            attributes.state = ::OpenVFS::Constants::States::Hydrating;
            break;
        case ItemTypeVirtualFile:
            [[fallthrough]];
        case ItemTypeVirtualFileDehydration:
            qCDebug(lcOpenVFS) << "updateMetadata for virtual file " << syncItem._type;
            attributes.state = ::OpenVFS::Constants::States::DeHydrated;
            break;
        case ItemTypeFile:
            // hydrated files must not have a size attribute != 0
            attributes.size = 0;
            [[fallthrough]];
        case ItemTypeDirectory:
            qCDebug(lcOpenVFS) << "updateMetadata for" << syncItem._type;
            attributes.state = ::OpenVFS::Constants::States::Hydrated;
            break;
        // case ItemTypeSymLink:
        //     [[fallthrough]];
        // case ItemTypeUnsupported:
        //     Q_UNREACHABLE();
        }

        if (const auto result = setPlaceholderAttributes(attributes, syncItem._modtime); !result) {
            qCCritical(lcOpenVFS) << "Failed to update placeholder for" << filePath << result.error();
            return result.error();
        }
        return ConvertToPlaceholderResult::Ok;
    }
}

void OpenVFS::slotHydrateJobFinished()
{
    HydrationJob *hydration = qobject_cast<HydrationJob *>(sender());

    const auto targetPath = FileSystem::toFilesystemPath(hydration->targetFileName());
    Q_ASSERT(!targetPath.empty());

    qCInfo(lcOpenVFS) << u"Hydration Job finished for" << targetPath.native();

    if (std::filesystem::exists(targetPath)) {
        auto item = OCC::SyncFileItem::fromSyncJournalFileRecord(hydration->record());
        // the file is now downloaded
        item->_type = ItemTypeFile;

        if (auto inode = 0ull;FileSystem::getInode(QString::fromStdWString(targetPath.wstring()), &inode)) {
            item->_inode = inode;
        } else {
            qCWarning(lcOpenVFS) << u"Failed to get inode for" << targetPath.native();
        }
        // Update the client sync journal database if the file modifications have been successful
        const auto result = this->params().journal->setFileRecord(SyncFileItem::fromSyncFileItem(*item));
        if (!result) {
            qCWarning(lcOpenVFS) << u"Error when setting the file record to the database" << result.error();
        } else {
            qCInfo(lcOpenVFS) << u"Hydration succeeded" << targetPath.native();
        }
    } else {
        qCWarning(lcOpenVFS) << u"Hydration succeeded but the file appears to be moved" << targetPath.native();
    }

    hydration->deleteLater();
    this->_hydrationJobs.remove(hydration->fileId());
}

Result<void, QString> OpenVFS::createPlaceholder(const SyncFileItem &item)
{
    const auto path = params().root() / item.localName();
    if (path.exists()) {
        Q_ASSERT(item._type == ItemTypeVirtualFileDehydration);
        // if (item._type == ItemTypeVirtualFileDehydration && FileSystem::fileChanged(path, FileSystem::FileChangedInfo::fromSyncFileItem(&item))) {
        //     return tr("Cannot dehydrate a placeholder because the file changed");
        // }
    }
    QFile file(path.get());
    if (!file.open(QFile::ReadWrite | QFile::Truncate)) {
        return file.errorString();
    }
    file.write("");
    file.close();

    const auto attributes = ::OpenVFS::PlaceHolderAttributes::create(path, item._etag.toStdString(), item._fileId.toStdString(), item._size);
    return setPlaceholderAttributes(attributes, item._modtime);
}

HydrationJob *OpenVFS::hydrateFile(const QByteArray &fileId, const QString &targetPath)
{
    qCInfo(lcOpenVFS) << u"Requesting hydration for" << fileId;
    if (_hydrationJobs.contains(fileId)) {
        qCWarning(lcOpenVFS) << u"Ignoring hydration request for running hydration for fileId" << fileId;
        return {};
    }

    if (auto attr = placeHolderAttributes(targetPath)) {
        attr.state = ::OpenVFS::Constants::States::Hydrating;
        if (auto res = setPlaceholderAttributes(attr); !res) {
            qCWarning(lcOpenVFS) << u"Failed to set attributes for" << targetPath << res.error();
            return nullptr;
        }
    } else {
        qCWarning(lcOpenVFS) << u"Failed to get attributes for" << targetPath;
        return nullptr;
    }
    HydrationJob *hydration = new HydrationJob(this, fileId, std::make_unique<QFile>(targetPath), nullptr);
    hydration->setTargetFile(targetPath);
    _hydrationJobs.insert(fileId, hydration);

    connect(hydration, &HydrationJob::finished, this, &OpenVFS::slotHydrateJobFinished);
    connect(hydration, &HydrationJob::error, this, [this, hydration](const QString &error) {
        qCWarning(lcOpenVFS) << u"Hydration failed" << error;
        this->_hydrationJobs.remove(hydration->fileId());
        hydration->deleteLater();
    });

    return hydration;
}

QString OpenVFS::fileSuffix() const
{
    return {};
}

bool OpenVFS::isHydrating() const
{
    return false;
}

Result<Vfs::ConvertToPlaceholderResult, QString> OpenVFS::updatePlaceholderMarkInSync([[maybe_unused]] const QString &filePath, [[maybe_unused]] const SyncFileItem &item)
{
    return {Vfs::ConvertToPlaceholderResult::Error};
}

bool OpenVFS::isPlaceHolderInSync([[maybe_unused]] const QString &filePath) const
{
    return false;
}

Result<void, QString> OpenVFS::createPlaceholders([[maybe_unused]] const QList<SyncFileItemPtr> &items)
{
    return {};
}

Result<void, QString> OpenVFS::dehydratePlaceholder([[maybe_unused]] const SyncFileItem &item)
{
    return {};
}

Result<Vfs::ConvertToPlaceholderResult, QString> OpenVFS::convertToPlaceholder(const QString &, const SyncFileItem &, const QString &, UpdateMetadataTypes)
{
    return {Vfs::ConvertToPlaceholderResult::Error};
}

bool OpenVFS::needsMetadataUpdate(const SyncFileItem &item)
{
    const auto path = params().root() / item.localName();
    // if the attributes do not exist we need to add them
    return QFileInfo::exists(path.toString()) && !placeHolderAttributes(path);
}

bool OpenVFS::isDehydratedPlaceholder(const QString &filePath)
{
    if (QFileInfo::exists(filePath)) {
        return placeHolderAttributes(filePath).state == ::OpenVFS::Constants::States::DeHydrated;
    }
    return false;
}

// LocalInfo OpenVFS::statTypeVirtualFile(const std::filesystem::directory_entry &path, ItemType type)
// {
//     if (type == ItemTypeFile) {
//         const auto attribs = placeHolderAttributes(path.path());
//         if (attribs.state == ::OpenVFS::Constants::States::DeHydrated) {
//             type = ItemTypeVirtualFile;
//             if (attribs.pinState == convertPinState(PinState::AlwaysLocal)) {
//                 type = ItemTypeVirtualFileDownload;
//             }
//         } else {
//             if (attribs.pinState == convertPinState(PinState::OnlineOnly)) {
//                 type = ItemTypeVirtualFileDehydration;
//             }
//         }
//     }
//     qCDebug(lcOpenVFS) << path.path().native() << Utility::enumToString(type);
//     return LocalInfo(path, type);
// }

bool OpenVFS::setPinState(const QString &folderPath, PinState state)
{
    const auto localPath = params().root() / folderPath;
    qCDebug(lcOpenVFS) << localPath.toString() << state;
    auto attribs = placeHolderAttributes(localPath);
    if (!attribs) {
        // the file is not yet converted
        return false;
    }
    attribs.pinState = convertPinState(state);
    if (!setPlaceholderAttributes(attribs)) {
        return false;
    }
    return true;
}

Optional<PinState> OpenVFS::pinState(const QString &folderPath)
{
    for (auto relativePath = FileSystem::Path::relative(folderPath).get();; relativePath = relativePath.parent_path()) {
        const auto attributes = placeHolderAttributes(params().root() / relativePath);
        if (!attributes) {
            qCDebug(lcOpenVFS) << "Couldn't find pin state for placeholder file" << folderPath;
            return {};
        }
        // if the state is inherited and we still have a parent path, retreive that instead.
        if (attributes.pinState != ::OpenVFS::Constants::PinStates::Inherited || !relativePath.has_relative_path()) {
            return convertPinState(attributes.pinState);
        }
    }
}

Vfs::AvailabilityResult OpenVFS::availability(const QString &folderPath, [[maybe_unused]] const AvailabilityRecursivity recursiveCheck)
{
    const auto attribs = placeHolderAttributes(params().root() / folderPath);
    if (attribs) {
        switch (convertPinState(attribs.pinState)) {
        case OCC::PinState::AlwaysLocal:
            return VfsItemAvailability::AlwaysLocal;
        case OCC::PinState::OnlineOnly:
            return VfsItemAvailability::OnlineOnly;
        case OCC::PinState::Inherited: {
            switch (attribs.state) {
            case ::OpenVFS::Constants::States::Hydrated:
                return VfsItemAvailability::AllHydrated;
            case ::OpenVFS::Constants::States::DeHydrated:
                return VfsItemAvailability::AllDehydrated;
            case ::OpenVFS::Constants::States::Hydrating:
                return VfsItemAvailability::Mixed;
            }
        }
            Q_UNREACHABLE();
        case OCC::PinState::Unspecified:
            [[fallthrough]];
        case OCC::PinState::Excluded:
            return VfsItemAvailability::Mixed;
        };
    } else {
        return AvailabilityError::NoSuchItem;
    }
    return VfsItemAvailability::Mixed;
}

void OpenVFS::fileStatusChanged(const QString &systemFileName, SyncFileStatus fileStatus)
{
    if (fileStatus.tag() == SyncFileStatus::StatusExcluded) {
        const FileSystem::Path rel = FileSystem::Path(systemFileName)->lexically_relative(params().root());
        setPinState(rel.toString(), PinState::Excluded);
        return;
    }
    qCDebug(lcOpenVFS) << systemFileName << fileStatus;
}


} // namespace OCC
