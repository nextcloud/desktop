/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
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

#include <QCryptographicHash>
#include <QString>

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102921
// we hope it will be fixed in GCC >= 13
#if !defined(Q_CC_GNU) || Q_CC_GNU >= 1300
#define constexpr_list constexpr
#else
#define constexpr_list inline
#endif
namespace OCC {
namespace CheckSums {
    OCSYNC_EXPORT Q_NAMESPACE;

    enum class Algorithm {
        NONE,
        SHA3_256 = QCryptographicHash::Sha3_256,
        SHA256 = QCryptographicHash::Sha256,
        SHA1 = QCryptographicHash::Sha1,
        MD5 = QCryptographicHash::Md5,
        ADLER32 = 100,
        DUMMY_FOR_TESTS,
        PARSE_ERROR
    };
    Q_ENUM_NS(Algorithm);

    constexpr std::string_view toString(Algorithm algo)
    {
        switch (algo) {
        case Algorithm::SHA3_256:
            return "SHA3-256";
        case Algorithm::SHA256:
            return "SHA256";
        case Algorithm::SHA1:
            return "SHA1";
        case Algorithm::MD5:
            return "MD5";
        case Algorithm::ADLER32:
            return "ADLER32";
        case Algorithm::DUMMY_FOR_TESTS:
            return "DUMMY_FOR_TESTS";
        case Algorithm::PARSE_ERROR:
            break;
        case Algorithm::NONE:
            // while none is a valid enum value, it is not valid when used to create a checksum string
            Q_UNREACHABLE();
            break;
        }
        return "ERROR";
    }

    inline QString toQString(Algorithm algo)
    {
        const auto n = toString(algo);
        return QString::fromUtf8(n.data(), n.size());
    }

    constexpr auto pair(Algorithm a)
    {
        return std::make_pair(a, toString(a));
    }

    constexpr_list auto All = {
        // Sorted by priority
        pair(Algorithm::SHA3_256),
        pair(Algorithm::SHA256),
        pair(Algorithm::SHA1),
        pair(Algorithm::MD5),
        pair(Algorithm::ADLER32),
        pair(Algorithm::DUMMY_FOR_TESTS)
    };

    constexpr_list auto UnsafeAlgorithms = {
        pair(Algorithm::ADLER32),
        pair(Algorithm::DUMMY_FOR_TESTS)
    };

    constexpr_list auto SafeAlgorithms = {
        pair(Algorithm::SHA3_256),
        pair(Algorithm::SHA256),
        pair(Algorithm::SHA1),
        pair(Algorithm::MD5)
    };

    inline Algorithm fromName(std::string_view s)
    {
        auto it = std::find_if(All.begin(), All.end(), [s](const auto &it) {
            return it.second == s;
        });
        if (it != All.end()) {
            return it->first;
        }
        return Algorithm::PARSE_ERROR;
    }

    OCSYNC_EXPORT Algorithm fromByteArray(const QByteArray &s);
}
}
