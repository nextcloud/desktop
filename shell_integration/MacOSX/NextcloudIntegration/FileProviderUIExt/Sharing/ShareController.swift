//
//  ShareController.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import Combine
import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import OSLog

class ShareController: ObservableObject {
    @Published private(set) var share: NKShare
    private let kit: NextcloudKit
    private let account: Account
    let log: any FileProviderLogging
    let logger: FileProviderLogger

    static func create(
        account: Account,
        kit: NextcloudKit,
        shareType: NKShare.ShareType,
        itemServerRelativePath: String,
        shareWith: String?,
        password: String? = nil,
        expireDate: String? = nil,
        permissions: NKShare.Permission,
        publicUpload: Bool,
        note: String? = nil,
        label: String? = nil,
        hideDownload: Bool,
        attributes: String? = nil,
        options: NKRequestOptions = NKRequestOptions()
    ) async -> NKError? {
        let sharee: String? = if shareType == .publicLink {
            nil
        } else {
            shareWith
        }

        if shareType != .publicLink, sharee == nil {
            // Any share requires a recipient, except public links.
            return NKError(statusCode: 0, fallbackDescription: "No recipient for share!")
        }

        let (_, _, _, error) = await kit.createShareAsync(
            path: itemServerRelativePath,
            shareType: shareType.rawValue,
            shareWith: sharee,
            hideDownload: hideDownload,
            password: password,
            permissions: permissions.rawValue,
            account: account.ncKitAccount,
            options: options
        )

        return error
    }

    init(share: NKShare, account: Account, kit: NextcloudKit, log: any FileProviderLogging) {
        self.account = account
        self.share = share
        self.kit = kit
        self.log = log
        self.logger = FileProviderLogger(category: "ShareController", log: log)
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
        logger.info("Saving share.", [.url: self.share.url])

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
                account: account.ncKitAccount,
                options: options
            ) { account, share, data, error in
                self.logger.info("Received update response: \(share?.url ?? "")")

                defer {
                    continuation.resume(returning: error)
                }

                guard error == .success, let share = share else {
                    self.logger.error("Error updating save.", [.error: error])
                    return
                }

                self.share = share
            }
        }
    }

    func delete() async -> NKError? {
        logger.info("Deleting share: \(self.share.url)")

        return await withCheckedContinuation { continuation in
            kit.deleteShare(idShare: share.idShare, account: account.ncKitAccount) { account, _, error in
                self.logger.info("Received delete response: \(self.share.url)")

                defer {
                    continuation.resume(returning: error)
                }

                guard error == .success else {
                    self.logger.error("Error deleting save: \(error.errorDescription)")

                    return
                }
            }
        }
    }
}
