/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountstate.h"

class QString;

// Forward declarations for Objective-C types using conditional compilation
#ifdef __OBJC__
@class NSFileProviderDomain;
@class NSFileProviderManager;
@class NSString;
#else
// In C++ context, use opaque pointers
struct NSFileProviderDomain;
struct NSFileProviderManager;
struct NSString;
#endif

/**
 * This file contains the FileProviderUtils namespace, which contains
 * utility functions for the file provider extension.
 *
 * Unlike other classes or namespaces in this module, this does not have
 * a clear file separation between C++ and Objective C++ code.
 * This is intended as a completely Objective-C++ namespace!
 *
 * You should threfore try to avoid using this in C++ code wherever possible
 * and only use this in *_mac.mm implementation files.
 */

namespace OCC {

namespace Mac {

namespace FileProviderUtils {

static constexpr auto bundleExtensions = std::array{
    QLatin1StringView(".app"),
    QLatin1StringView(".framework"),
    QLatin1StringView(".kext"),
    QLatin1StringView(".plugin"),
    QLatin1StringView(".docset"),
    QLatin1StringView(".xpc"),
    QLatin1StringView(".qlgenerator"),
    QLatin1StringView(".component"),
    QLatin1StringView(".saver"),
    QLatin1StringView(".mdimporter")
};

static const QRegularExpression illegalChars("[:/]");

/**
 * @brief Whether the given domain identifier contains illegal characters or a known bundle extension.
 * @todo Refactor this to validate file provider domain display names instead because identifiers are all UUIDs by now.
 */
bool illegalDomainIdentifier(const QString &domainId);

/**
 * @brief Find the logs directory of the file provider domain with the given identifier.
 */
QDir fileProviderDomainLogDirectory(const QString domainIdentifier);

/**
 * @brief Find the application support directory for all the file provider domains.
 */
QDir fileProviderDomainsSupportDirectory();

/**
 * @brief Find the application support directory of the file provider domain with the given identifier.
 */
QDir fileProviderDomainSupportDirectory(const QString domainIdentifier);

/**
 * @brief Synchronously retrieves an NSFileProviderManager for the given domain identifier.
 *
 * Domain enumeration waits up to five seconds for a reply.
 * @return The manager if found, or nil when enumeration fails, times out, or finds no matching domain.
 * The returned manager follows standard Objective-C ownership; the caller does not release it.
 */
NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier);

QString groupContainerPath();

} // namespace FileProviderUtils

} // namespace Mac

} // namespace OCC
