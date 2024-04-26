//
//  RemoteRequestOptions.swift
//
//
//  Created by Claudio Cambra on 26/4/24.
//

import Dispatch
import Foundation

public struct RemoteRequestOptions {
    var endpoint: String?
    var version: String?
    var customHeader: [String: String]?
    var customUserAgent: String?
    var contentType: String?
    var e2eToken: String?
    var timeout: TimeInterval
    var queue: DispatchQueue

    public init(
        endpoint: String? = nil,
        version: String? = nil,
        customHeader: [String : String]? = nil,
        customUserAgent: String? = nil,
        contentType: String? = nil,
        e2eToken: String? = nil,
        timeout: TimeInterval = 60,
        queue: DispatchQueue = .main
    ) {
        self.endpoint = endpoint
        self.version = version
        self.customHeader = customHeader
        self.customUserAgent = customUserAgent
        self.contentType = contentType
        self.e2eToken = e2eToken
        self.timeout = timeout
        self.queue = queue
    }
}
