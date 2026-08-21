/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

//
// The XPC contract is declared in Objective-C because it has three consumers in two
// languages: the Objective-C++ desktop client, the Objective-C FinderSync extension, and
// this Swift broker. NSXPCInterface requires an @objc protocol, and a Swift-declared one
// would only be visible to the other targets through a generated -Swift.h that they do not
// build against.
//

#import "FinderSyncBrokerProtocol.h"
#import "FinderSyncBrokerClientProtocol.h"
