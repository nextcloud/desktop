//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import NextcloudKit

public enum UploadFailure: Error {
    case destinationCollision(NKError)
    case other(NKError)

    var underlyingError: NKError {
        switch self {
            case let .destinationCollision(error), let .other(error):
                error
        }
    }
}
