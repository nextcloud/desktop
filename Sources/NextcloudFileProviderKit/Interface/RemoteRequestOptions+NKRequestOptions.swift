//
//  RemoteRequestOptions+NKRequestOptions.swift
//
//
//  Created by Claudio Cambra on 26/4/24.
//

import Foundation
import NextcloudKit

extension RemoteRequestOptions {
    func toNKRequestOptions() -> NKRequestOptions {
        NKRequestOptions(
            endpoint: endpoint,
            version: version,
            customHeader: customHeader,
            customUserAgent: customUserAgent,
            contentType: contentType,
            e2eToken: e2eToken,
            timeout: timeout,
            queue: queue
        )
    }
}
