//  SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

import Foundation
@testable import NextcloudFileProviderKit
import Testing

@Suite("Unified sharing capability")
struct UnifiedSharingCapabilityTests {
    @Test("Detects the sharing capability")
    func detectsSharingCapability() {
        let responseData = Data(#"{"ocs":{"data":{"capabilities":{"sharing":{}}}}}"#.utf8)

        #expect(UnifiedSharingCapability.isAvailable(in: responseData))
    }

    @Test("Treats the presence of a null sharing capability as available")
    func detectsNullSharingCapability() {
        let responseData = Data(#"{"ocs":{"data":{"capabilities":{"sharing":null}}}}"#.utf8)

        #expect(UnifiedSharingCapability.isAvailable(in: responseData))
    }

    @Test("Rejects a capabilities response without sharing")
    func rejectsMissingSharingCapability() {
        let responseData = Data(#"{"ocs":{"data":{"capabilities":{"files":{}}}}}"#.utf8)

        #expect(!UnifiedSharingCapability.isAvailable(in: responseData))
    }

    @Test("Rejects malformed capability data")
    func rejectsMalformedCapabilityData() {
        #expect(!UnifiedSharingCapability.isAvailable(in: Data("not json".utf8)))
        #expect(!UnifiedSharingCapability.isAvailable(in: nil))
    }
}
