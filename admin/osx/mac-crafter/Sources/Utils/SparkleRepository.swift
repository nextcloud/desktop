//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: GPL-2.0-or-later

import Foundation

///
/// Facility which abstracts and implements the Sparkle framework retrieval.
///
enum SparkleRepository {
    ///
    /// The default release of Sparkle to use.
    ///
    static let defaultRelease = "2.9.1"

    private struct GitHubRelease: Decodable {
        let tagName: String

        enum CodingKeys: String, CodingKey {
            case tagName = "tag_name"
        }
    }

    ///
    /// Retrieve `https://api.github.com/repos/sparkle-project/Sparkle/releases/latest` and extract the `tag_name` property value.
    ///
    /// - Returns: `tag_name` of the latest release Sparkle available on GitHub.
    ///
    static func fetchLatestReleaseTagName() async throws -> String {
        let url = URL(string: "https://api.github.com/repos/sparkle-project/Sparkle/releases/latest")!
        let (data, _) = try await URLSession.shared.data(from: url)
        let release = try JSONDecoder().decode(GitHubRelease.self, from: data)
        return release.tagName
    }

    ///
    /// Compare `release` against the return value of ``fetchLatestReleaseTagName()``.
    ///
    static func isLatestRelease(_ release: String) async {
        do {
            let latestReleaseTagName = try await fetchLatestReleaseTagName()

            if release == latestReleaseTagName {
                Log.info("The Sparkle release \(Self.defaultRelease) to use is the latest.")
            } else {
                Log.info("The Sparkle release \(Self.defaultRelease) to use is outdated, \(latestReleaseTagName) is the latest.")
            }
        } catch {
            Log.error("Failed to fetch latest Sparkle release: \(error)")
        }
    }

    ///
    /// Fetch the assets of the latest release and look for a matching
    ///
    static func getDownloadAddress(for release: String) async -> URL {
        await isLatestRelease(release)

        return URL(string: "https://github.com/sparkle-project/Sparkle/releases/download/\(release)/Sparkle-\(release).tar.xz")!
    }
}
