/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

import Foundation
import FileProvider
import NextcloudKit

extension NKError {
    static var noChangesErrorCode: Int {
        return -200
    }

    var isCouldntConnectError: Bool {
        return errorCode == -9999 ||
            errorCode == -1001 ||
            errorCode == -1004 ||
            errorCode == -1005 ||
            errorCode == -1009 ||
            errorCode == -1012 ||
            errorCode == -1200 ||
            errorCode == -1202 ||
            errorCode == 500 ||
            errorCode == 503 ||
            errorCode == 200
    }

    var isUnauthenticatedError: Bool {
        return errorCode == -1013
    }

    var isGoingOverQuotaError: Bool {
        return errorCode == 507
    }

    var isNotFoundError: Bool {
        return errorCode == 404
    }

    var isNoChangesError: Bool {
        return errorCode == NKError.noChangesErrorCode
    }

    var fileProviderError: NSFileProviderError {
        if isNotFoundError {
            return NSFileProviderError(.noSuchItem)
        } else if isCouldntConnectError {
            // Provide something the file provider can do something with
            return NSFileProviderError(.serverUnreachable)
        } else if isUnauthenticatedError {
            return NSFileProviderError(.notAuthenticated)
        } else if isGoingOverQuotaError {
            return NSFileProviderError(.insufficientQuota)
        } else {
            return NSFileProviderError(.cannotSynchronize)
        }
    }
}
