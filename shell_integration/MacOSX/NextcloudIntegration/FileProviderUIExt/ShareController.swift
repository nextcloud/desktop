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

    func save() async -> NKError? {
        return await withCheckedContinuation { continuation in
            kit.updateShare(
                idShare: share.idShare,
                password: share.password,
                expireDate: share.expirationDateString ?? "",
                permissions: share.permissions,
                note: share.note,
                label: share.label,
                hideDownload: share.hideDownload,
                attributes: share.attributes
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
}
