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
    func toFileProviderError() -> NSFileProviderError {
        let nkErrorCode = self.errorCode

        if nkErrorCode == 404 {
            return NSFileProviderError(.noSuchItem)
        } else if nkErrorCode == -9999 || nkErrorCode == -1001 || nkErrorCode == -1004 || nkErrorCode == -1005 || nkErrorCode == -1009 || nkErrorCode == -1012 || nkErrorCode == -1200 || nkErrorCode == -1202 || nkErrorCode == 500 || nkErrorCode == 503 || nkErrorCode == 200 {
            // Provide something the file provider can do something with
            return NSFileProviderError(.serverUnreachable)
        } else if nkErrorCode == -1013  {
            return NSFileProviderError(.notAuthenticated)
        } else if nkErrorCode == 507 {
            return NSFileProviderError(.insufficientQuota)
        } else {
            return NSFileProviderError(.cannotSynchronize)
        }
    }
}
