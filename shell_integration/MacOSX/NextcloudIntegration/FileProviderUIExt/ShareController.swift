//
//  ShareController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 4/3/24.
//

import Combine
import Foundation
import NextcloudKit
import OSLog

class ShareController: ObservableObject {
    @Published private(set) var share: NKShare
    private let kit: NextcloudKit

    init(share: NKShare, kit: NextcloudKit) {
        self.share = share
        self.kit = kit
    }

    func save(
        password: String? = nil,
        expireDate: String? = nil,
        permissions: Int = 1,
        publicUpload: Bool = false,
        note: String? = nil,
        label: String? = nil,
        hideDownload: Bool,
        attributes: String? = nil,
        options: NKRequestOptions = NKRequestOptions()
    ) async -> NKError? {
        Logger.shareController.info("Saving share: \(self.share.url)")
        return await withCheckedContinuation { continuation in
            kit.updateShare(
                idShare: share.idShare,
                password: password,
                expireDate: expireDate,
                permissions: permissions,
                publicUpload: publicUpload,
                note: note,
                label: label,
                hideDownload: hideDownload,
                attributes: attributes,
                options: options
            ) { account, share, data, error in
                Logger.shareController.info("Received update response: \(share?.url ?? "")")
                defer { continuation.resume(returning: error) }
                guard error == .success, let share = share else {
                    Logger.shareController.error("Error updating save: \(error.errorDescription)")
                    return
                }
                self.share = share
            }
        }
    }

    func delete() async -> NKError? {
        Logger.shareController.info("Deleting share: \(self.share.url)")
        return await withCheckedContinuation { continuation in
            kit.deleteShare(idShare: share.idShare) { account, error in
                Logger.shareController.info("Received delete response: \(self.share.url)")
                defer { continuation.resume(returning: error) }
                guard error == .success else {
                    Logger.shareController.error("Error deleting save: \(error.errorDescription)")
                    return
                }
            }
        }
    }
}
