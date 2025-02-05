//
//  ShareeSuggestionsDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 2/4/24.
//

import Foundation
import NextcloudFileProviderKit
import NextcloudKit
import OSLog
import SuggestionsTextFieldKit

class ShareeSuggestionsDataSource: SuggestionsDataSource {
    let kit: NextcloudKit
    let account: Account
    var suggestions: [Suggestion] = []
    var inputString: String = "" {
        didSet { Task { await updateSuggestions() } }
    }

    init(account: Account, kit: NextcloudKit) {
        self.account = account
        self.kit = kit
    }

    private func updateSuggestions() async {
        let sharees = await fetchSharees(search: inputString)
        Logger.shareeDataSource.info("Fetched \(sharees.count, privacy: .public) sharees.")
        suggestions = suggestionsFromSharees(sharees)
        NotificationCenter.default.post(name: SuggestionsChangedNotificationName, object: self)
    }

    private func fetchSharees(search: String) async -> [NKSharee] {
        Logger.shareeDataSource.debug("Searching sharees with: \(search, privacy: .public)")
        return await withCheckedContinuation { continuation in
            kit.searchSharees(
                search: inputString,
                page: 1,
                perPage: 20,
                account: account.ncKitAccount,
                completion: { account, sharees, data, error in
                    defer { continuation.resume(returning: sharees ?? []) }
                    guard error == .success else {
                        Logger.shareeDataSource.error(
                            "Error fetching sharees: \(error.errorDescription, privacy: .public)"
                        )
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
