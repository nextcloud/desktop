//
//  RemoteErrorType.swift
//
//
//  Created by Claudio Cambra on 26/4/24.
//

import Foundation

public enum RemoteErrorType {
    case unknown
    case unauthorised
    case notFound
    case couldNotConnect
    case overQuota
    case noChanges
    case collision

    init(code: Int) {
        switch code {
        case -1013, 401, 403:
            self = .unauthorised
        case 404:
            self = .notFound
        case 405:
            self = .collision
        case 507:
            self = .overQuota
        case -200:
            self = .noChanges
        case -9999, -1001, -1004, -1005, -1009, -1012, -1200, -1202, 500, 503, 200:
            self = .couldNotConnect
        default:
            self = .unknown
        }
    }
}
