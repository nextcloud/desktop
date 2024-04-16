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

import Alamofire
import Foundation

extension Progress {
    func setHandlersFromAfRequest(_ request: Request) {
        cancellationHandler = { request.cancel() }
        pausingHandler = { request.suspend() }
        resumingHandler = { request.resume() }
    }

    func copyCurrentStateToProgress(_ otherProgress: Progress, includeHandlers: Bool = false) {
        if includeHandlers {
            otherProgress.cancellationHandler = cancellationHandler
            otherProgress.pausingHandler = pausingHandler
            otherProgress.resumingHandler = resumingHandler
        }

        otherProgress.totalUnitCount = totalUnitCount
        otherProgress.completedUnitCount = completedUnitCount
        otherProgress.estimatedTimeRemaining = estimatedTimeRemaining
        otherProgress.localizedDescription = localizedAdditionalDescription
        otherProgress.localizedAdditionalDescription = localizedAdditionalDescription
        otherProgress.isCancellable = isCancellable
        otherProgress.isPausable = isPausable
        otherProgress.fileCompletedCount = fileCompletedCount
        otherProgress.fileURL = fileURL
        otherProgress.fileTotalCount = fileTotalCount
        otherProgress.fileCompletedCount = fileCompletedCount
        otherProgress.fileOperationKind = fileOperationKind
        otherProgress.kind = kind
        otherProgress.throughput = throughput

        for (key, object) in userInfo {
            otherProgress.setUserInfoObject(object, forKey: key)
        }
    }

    func copyOfCurrentState(includeHandlers: Bool = false) -> Progress {
        let newProgress = Progress()
        copyCurrentStateToProgress(newProgress, includeHandlers: includeHandlers)
        return newProgress
    }
}
