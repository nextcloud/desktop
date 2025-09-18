/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "accountstate.h"

class QString;

@class NSFileProviderDomain;
@class NSFileProviderManager;

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
 *
 * IMPORTANT: All Objective-C objects returned here need to be released!
 * They have been internally retained due to the async nature of the
 * FileProvider API.
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
 * @brief Synchronously retrieves an NSFileProviderDomain for the given domain identifier.
 *
 * This function searches through all registered file provider domains and returns the one
 * matching the provided identifier. The function blocks until the asynchronous file provider
 * API call completes using a dispatch semaphore.
 *
 * @param domainIdentifier The unique identifier of the domain to find
 * @return A retained NSFileProviderDomain object if found, nil otherwise
 *
 * @warning The returned NSFileProviderDomain has been retained and MUST be released by the caller!
 * @warning This function blocks the calling thread and should not be called frequently
 *          due to its synchronous nature over an asynchronous API
 */
// Synchronous function to get the domain for a domain identifier
NSFileProviderDomain *domainForIdentifier(const QString &domainIdentifier);

/**
 * @brief Resolve a file provider domain identifier by the given account.
 *
 * This function checks the list of known identifiers to contain one for the given account
 * and falls back to deterministic legacy identifiers derived from the account identifier.
 */
QString domainIdentifierForAccount(const OCC::Account * const account);

/**
 * @brief Resolve a file provider domain identifier by the given account pointer.
 */
QString domainIdentifierForAccount(const OCC::AccountPtr account);

/**
 * @brief Resolve a file provider domain identifier by the given account identifier.
 */
QString domainIdentifierForAccountIdentifier(const QString &accountId);

/**
 * @brief Resolve a file provider domain identifier by the given account identifier.
 */
QString domainIdentifierForAccountIdentifier(const NSString *accountId);

/**
 * @brief Whether the given domain identifier contains illegal characters or a known bundle extension.
 */
bool illegalDomainIdentifier(const QString &domainId);

/**
 * @brief Synchronously retrieves an NSFileProviderManager for the given domain identifier.
 *
 * This function first finds the NSFileProviderDomain using domainForIdentifier, then
 * creates and returns the corresponding NSFileProviderManager. The function handles
 * proper memory management by releasing the intermediate domain object.
 *
 * @param domainIdentifier The unique identifier of the domain to get the manager for
 * @return An NSFileProviderManager object for the domain if found, nil otherwise
 *
 * @warning This function blocks the calling thread due to its dependency on domainForIdentifier
 * @warning The caller does NOT need to release the returned NSFileProviderManager
 *          (it follows standard Objective-C memory management)
 *
 * @see domainForIdentifier for domain lookup implementation details
 */
// Synchronous function to get manager for a domain identifier
NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier);

QString groupContainerPath();

} // namespace FileProviderUtils

} // namespace Mac

} // namespace OCC
