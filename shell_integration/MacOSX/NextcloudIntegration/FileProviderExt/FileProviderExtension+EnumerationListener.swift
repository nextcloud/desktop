//
//  FileProviderExtension+EnumerationListener.swift
//  FileProviderExt
//
//  Created by Claudio Cambra on 16/7/24.
//

import Foundation
import NextcloudFileProviderKit

extension FileProviderExtension: EnumerationListener {
    func enumerationActionStarted(actionId: UUID) {
        insertSyncAction(actionId)
    }

    func enumerationActionFinished(actionId: UUID) {
        removeSyncAction(actionId)
    }

    func enumerationActionFailed(actionId: UUID, error: Error) {
        insertErrorAction(actionId)
    }
}
