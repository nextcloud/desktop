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
    var inputString: String = "" {
        didSet {
            Task {
                await updateSuggestions()
            }
        }
    }

    init(account: Account, kit: NextcloudKit, log: any FileProviderLogging) {
        self.account = account
        self.kit = kit
        self.logger = FileProviderLogger(category: "ShareeSuggestionsDataSource", log: log)
    }

    private func updateSuggestions() async {
        let sharees = await fetchSharees(search: inputString)
        logger.info("Fetched \(sharees.count) sharees.")
        suggestions = suggestionsFromSharees(sharees)
        NotificationCenter.default.post(name: SuggestionsChangedNotificationName, object: self)
    }

    private func fetchSharees(search: String) async -> [NKSharee] {
        logger.debug("Searching sharees with: \(search)")

        return await withCheckedContinuation { continuation in
            kit.searchSharees(
                search: inputString,
                page: 1,
                perPage: 20,
                account: account.ncKitAccount,
                completion: { account, sharees, data, error in
                    defer {
                        continuation.resume(returning: sharees ?? [])
                    }

                    guard error == .success else {
                        self.logger.error("Error fetching sharees: \(error.errorDescription)")
                        return
                    }
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
