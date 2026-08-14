// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Hannah von Reth <h.vonreth@opencloud.eu>

#include "path.h"

#include "common/filesystembase.h"

OCC::FileSystem::Path::Path(QStringView path)
    : _path(toFilesystemPath(path.toString()))
{
}

OCC::FileSystem::Path::Path(const std::filesystem::path &path)
    : _path(path)
{
}

OCC::FileSystem::Path::Path(std::filesystem::path &&path)
    : _path(std::move(path))
{
}

OCC::FileSystem::Path OCC::FileSystem::Path::relative(QAnyStringView path)
{
    return QtPrivate::toFilesystemPath(path.toString());
}

QString OCC::FileSystem::Path::toString() const
{
    if (_path.empty()) {
        return {};
    }
    return fromFilesystemPath(_path);
}

bool OCC::FileSystem::Path::exists() const
{
    if (_path.empty()) {
        return false;
    }
    std::error_code ec;
    const bool exists = std::filesystem::exists(_path, ec);
    if (ec) {
        qCCritical(lcFileSystem) << u"Error checking existence of" << _path.native() << ec.value() << ec.message();
        return false;
    }
    return exists;
}

QDebug operator<<(QDebug debug, const OCC::FileSystem::Path &path)
{
    return debug << u"Path(" << path.toString() << u")";
}
