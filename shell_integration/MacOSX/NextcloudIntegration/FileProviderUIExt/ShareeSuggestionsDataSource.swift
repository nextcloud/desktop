//
//  ShareeSuggestionsDataSource.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 2/4/24.
//

import Foundation
import NextcloudKit
import OSLog
import SuggestionsTextFieldKit

class ShareeSuggestionsDataSource: SuggestionsDataSource {
    let kit: NextcloudKit
    var suggestions: [SuggestionsTextFieldKit.Suggestion] = []
    var inputString: String = ""

    init(kit: NextcloudKit) {
        self.kit = kit
    }
}
