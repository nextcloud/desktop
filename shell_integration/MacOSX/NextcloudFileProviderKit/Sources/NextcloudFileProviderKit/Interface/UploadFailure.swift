//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import NextcloudKit

public enum UploadFailure: Error {
    case destinationCollision(NKError, metadata: SendableItemMetadata? = nil)
    case other(NKError)

    var underlyingError: NKError {
        switch self {
            case let .destinationCollision(error, _), let .other(error):
                error
        }
    }
}
