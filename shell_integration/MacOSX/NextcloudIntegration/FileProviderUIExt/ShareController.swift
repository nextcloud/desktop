//
//  ShareController.swift
//  FileProviderUIExt
//
//  Created by Claudio Cambra on 4/3/24.
//

import Foundation
import NextcloudKit

class ShareController {
    let share: NKShare
    private let kit: NextcloudKit

    init(share: NKShare, kit: NextcloudKit) {
        self.share = share
        self.kit = kit
    }
}
