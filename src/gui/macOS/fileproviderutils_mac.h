/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#import <FileProvider/FileProvider.h>

static_assert(__has_feature(objc_arc), "File Provider helpers require ARC.");

namespace OCC::Mac::FileProviderUtils
{

using RequestReplyHandler = BOOL (^)(id result, NSError *error);

// Return whether all replies succeeded; results includes partial successes on timeout or error.
bool waitForFileProviderReplies(NSUInteger requestCount, const char *operation, void (^startRequests)(RequestReplyHandler), NSArray **results);

// Wait for one File Provider reply and ignore late or duplicate replies.
// A timeout does not cancel the request; result is only assigned after success.
bool waitForFileProviderRequest(const char *operation, void (^startRequest)(RequestReplyHandler), id *result = nullptr);
}
