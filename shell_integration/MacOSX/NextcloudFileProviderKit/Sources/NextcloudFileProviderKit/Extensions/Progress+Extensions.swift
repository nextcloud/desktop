//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

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
