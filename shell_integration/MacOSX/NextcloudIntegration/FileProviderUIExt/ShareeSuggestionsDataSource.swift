//
//  ShareeSuggestionsDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 2/4/24.
//

import Foundation
import NextcloudKit
import OSLog

class ShareeSuggestionsDataSource: SuggestionsDataSource {
    let kit: NextcloudKit
    var suggestions: [Suggestion] = []
    var inputString: String = "" {
        didSet { Task { await updateSuggestions() } }
    }

    init(kit: NextcloudKit) {
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
                completion: { account, sharees, data, error in
                    defer { continuation.resume(returning: sharees ?? []) }
                    guard error == .success else {
                        Logger.shareeDataSource.error(
                            "Error fetching sharees: \(error.description, privacy: .public)"
                        )
                        return
                    }
                }
            )
        }
    }

    private func suggestionsFromSharees(_ sharees: [NKSharee]) -> [Suggestion] {
        sharees.map { Suggestion(imageName: "person.fill", displayText: $0.name, data: $0) }
    } // TODO: Improve img
}
