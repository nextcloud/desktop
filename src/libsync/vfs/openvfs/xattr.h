// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Hannah von Reth <h.vonreth@opencloud.eu>

#pragma once

#include "owncloudlib.h"

#include "common/result.h"

#include <filesystem>
#include <optional>

#include <QString>

namespace OCC {
namespace FileSystem {
    namespace Xattr {
        OWNCLOUDSYNC_EXPORT bool supportsxattr(const std::filesystem::path &path);
        OWNCLOUDSYNC_EXPORT std::optional<QByteArray> getxattr(const std::filesystem::path &path, const QString &name);
        OWNCLOUDSYNC_EXPORT Result<void, QString> setxattr(const std::filesystem::path &path, const QString &name, const QByteArray &value);
        OWNCLOUDSYNC_EXPORT Result<void, QString> removexattr(const std::filesystem::path &path, const QString &name);
    }
}
}
