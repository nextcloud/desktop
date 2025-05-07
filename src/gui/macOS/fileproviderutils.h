/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

class QString;

@class NSFileProviderDomain;
@class NSFileProviderManager;

/**
 * This file contains the FileProviderUtils namespace, which contains
 * utility functions for the FileProvider extension.
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

// Synchronous function to get the domain for a domain identifier
NSFileProviderDomain *domainForIdentifier(const QString &domainIdentifier);

// Synchronous function to get manager for a domain identifier
NSFileProviderManager *managerForDomainIdentifier(const QString &domainIdentifier);

} // namespace FileProviderUtils

} // namespace Mac

} // namespace OCC
