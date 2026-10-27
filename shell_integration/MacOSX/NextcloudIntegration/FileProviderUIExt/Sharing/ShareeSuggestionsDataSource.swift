//
//  ShareeSuggestionsDataSource.swift
//  FileProviderUIExt
//
//  SPDX-FileCopyrightText: 2024 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later
//

import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import SuggestionsTextFieldKit

class ShareeSuggestionsDataSource: SuggestionsDataSource {
    let kit: NextcloudKit
    let logger: FileProviderLogger
    let account: Account
    var suggestions: [Suggestion] = []
    var shareType: NKShare.ShareType {
        didSet {
            guard shareType != oldValue else { return }
            inputString = ""
        }
    }
    var inputString: String = "" {
        didSet { updateSuggestions() }
    }
    private var searchIdentifier = UUID()

    init(account: Account, kit: NextcloudKit, log: any FileProviderLogging, shareType: NKShare.ShareType) {
        self.account = account
        self.kit = kit
        self.logger = FileProviderLogger(category: "ShareeSuggestionsDataSource", log: log)
        self.shareType = shareType
    }

    private func updateSuggestions() {
        let identifier = UUID()
        searchIdentifier = identifier
        let search = inputString
        let type = shareType
        suggestions = []
        NotificationCenter.default.post(name: SuggestionsChangedNotificationName, object: self)
        guard !search.isEmpty, type != .publicLink else { return }

        Task { @MainActor in
            let sharees = await fetchSharees(search: search)
            guard searchIdentifier == identifier else { return }
            suggestions = suggestionsFromSharees(sharees.filter { $0.shareType == type.rawValue })
            NotificationCenter.default.post(name: SuggestionsChangedNotificationName, object: self)
        }
    }

    private func fetchSharees(search: String) async -> [NKSharee] {
        logger.debug("Searching sharees with: \(search)")

        return await withCheckedContinuation { continuation in
            kit.searchSharees(
                search: search,
                page: 1,
                perPage: 20,
                account: account.ncKitAccount,
                completion: { account, sharees, data, error in
                    guard error == .success else {
                        self.logger.error("Error fetching sharees: \(error.errorDescription)")
                        continuation.resume(returning: [])
                        return
                    }
                    continuation.resume(returning: sharees ?? [])
                }
            )
        }
    }

    private func suggestionsFromSharees(_ sharees: [NKSharee]) -> [Suggestion] {
        return sharees.map {
            Suggestion(
                imageName: "person.fill",
                displayText: $0.label.isEmpty ? $0.name : $0.label,
                data: $0
            )
        }
    }
}
