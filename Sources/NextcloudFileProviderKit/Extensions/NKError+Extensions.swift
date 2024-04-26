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

import FileProvider
import Foundation
import NextcloudKit

extension NKError {

    var isCouldntConnectError: Bool { RemoteErrorType(code: errorCode) == .couldNotConnect }
    var isUnauthenticatedError: Bool { RemoteErrorType(code: errorCode) == .unauthorised }
    var isGoingOverQuotaError: Bool { RemoteErrorType(code: errorCode) == .overQuota }
    var isNotFoundError: Bool { RemoteErrorType(code: errorCode) == .notFound }
    var isNoChangesError: Bool { RemoteErrorType(code: errorCode) == .noChanges }
    var isUnauthorizedError: Bool { isUnauthenticatedError }
    var matchesCollisionError: Bool { RemoteErrorType(code: errorCode) == .collision }
    
    var fileProviderError: NSFileProviderError? {
        if self == .success {
            return nil
        }

        let errorType = RemoteErrorType(code: self.errorCode)
        switch (errorType) {
        case .notFound:
            return NSFileProviderError(.noSuchItem)
        case .couldNotConnect:
            // Provide something the file provider can do something with
            return NSFileProviderError(.serverUnreachable)
        case .unauthorised:
            return NSFileProviderError(.notAuthenticated)
        case .overQuota:
            return NSFileProviderError(.insufficientQuota)
        default:
            return NSFileProviderError(.cannotSynchronize)
        }
    }
}
