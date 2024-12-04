//
//  ShareController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 4/3/24.
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

    static func create(
        account: Account,
        kit: NextcloudKit,
        shareType: NKShare.ShareType,
        itemServerRelativePath: String,
        shareWith: String?,
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
        Logger.shareController.info("Creating share: \(itemServerRelativePath)")
        return await withCheckedContinuation { continuation in
            if shareType == .publicLink {
                kit.createShareLink(
                    path: itemServerRelativePath,
                    hideDownload: hideDownload,
                    publicUpload: publicUpload,
                    password: password,
                    permissions: permissions,
                    account: account.ncKitAccount,
                    options: options
                ) { account, share, data, error in
                    defer { continuation.resume(returning: error) }
                    guard error == .success else {
                        Logger.shareController.error(
                            """
                            Error creating link share: \(error.errorDescription, privacy: .public)
                            """
                        )
                        return
                    }
                }
            } else {
                guard let shareWith = shareWith else {
                    let errorString = "No recipient for share!"
                    Logger.shareController.error("\(errorString, privacy: .public)")
                    let error = NKError(statusCode: 0, fallbackDescription: errorString)
                    continuation.resume(returning: error)
                    return
                }

                kit.createShare(
                    path: itemServerRelativePath,
                    shareType: shareType.rawValue,
                    shareWith: shareWith,
                    password: password,
                    permissions: permissions,
                    attributes: attributes,
                    account: account.ncKitAccount
                ) { account, share, data, error in
                    defer { continuation.resume(returning: error) }
                    guard error == .success else {
                        Logger.shareController.error(
                            """
                            Error creating share: \(error.errorDescription, privacy: .public)
                            """
                        )
                        return
                    }
                }
            }
        }
    }

    init(share: NKShare, account: Account, kit: NextcloudKit) {
        self.account = account
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
        Logger.shareController.info("Saving share: \(self.share.url, privacy: .public)")
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
                Logger.shareController.info(
                    """
                    Received update response: \(share?.url ?? "", privacy: .public)
                    """
                )
                defer { continuation.resume(returning: error) }
                guard error == .success, let share = share else {
                    Logger.shareController.error(
                        """
                        Error updating save: \(error.errorDescription, privacy: .public)
                        """
                    )
                    return
                }
                self.share = share
            }
        }
    }

    func delete() async -> NKError? {
        Logger.shareController.info("Deleting share: \(self.share.url, privacy: .public)")
        return await withCheckedContinuation { continuation in
            kit.deleteShare(
                idShare: share.idShare, account: account.ncKitAccount
            ) { account, _, error in
                Logger.shareController.info(
                    """
                    Received delete response: \(self.share.url, privacy: .public)
                    """
                )
                defer { continuation.resume(returning: error) }
                guard error == .success else {
                    Logger.shareController.error(
                        """
                        Error deleting save: \(error.errorDescription, privacy: .public)
                        """
                    )
                    return
                }
            }
        }
    }
}
