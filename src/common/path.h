// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: 2025 Hannah von Reth <h.vonreth@opencloud.eu>

#pragma once

#include "ocsynclib.h"

#include <QString>

#include <filesystem>

namespace OCC {
namespace FileSystem {
    class OCSYNC_EXPORT Path
    {
    public:
        Path() = default;
        /**
         * Will create an absolute path from the given QString.
         * On Windows it will create a UNC path starting with "\\?\".
         * For path segments use Path::relative.
         * @param path
         */
        explicit Path(QStringView path);

        Path(const std::filesystem::path &path);

        Path(std::filesystem::path &&path);

        /**
         * Creates a relative path segment
         * @param path
         * @return  A relative path
         */
        static Path relative(QAnyStringView path);

        const std::filesystem::path &get() const { return _path; }

        [[nodiscard]] operator std::filesystem::path() const { return _path; }
        [[nodiscard]] operator const std::filesystem::path &() const { return _path; }
        const std::filesystem::path *operator->() const { return &_path; }

        explicit operator QString() const { return toString(); }

        Path operator/(const Path &other) const { return _path / other._path; }
        Path operator/(const std::filesystem::path &other) const { return _path / other; }
        Path operator/(QStringView other) const { return _path / relative(other); }

        Path &operator/=(const Path &other)
        {
            _path /= other._path;
            return *this;
        }

        Path &operator/=(const std::filesystem::path &other)
        {
            _path /= other;
            return *this;
        }


        Path &operator/=(QStringView other)
        {
            _path /= relative(other);
            return *this;
        }

        [[nodiscard]] QString toString() const;

        [[nodiscard]] bool exists() const;

    private:
        std::filesystem::path _path;
    };
}
}


OCSYNC_EXPORT QDebug operator<<(QDebug debug, const OCC::FileSystem::Path &path);
