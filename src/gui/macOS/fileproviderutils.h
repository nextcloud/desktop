/*
 * Copyright 2023 (c) Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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
