//
//  Upload.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 2024-12-29.
//

import Alamofire
import Foundation
import NextcloudKit

struct UploadResult {
    let ocId: String?
    let chunks: [RemoteFileChunk]?
    let etag: String?
    let date: Date?
    let size: Int64?
    let afError: AFError?
    let remoteError: NKError
    var succeeded: Bool { remoteError == .success }
}

