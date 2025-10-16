//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

///
/// An enum that can represent any JSON value and is `Encodable`.
///
/// > To Do: Add custom encodings for:
/// > - `RealmItemMetadata`
///
public enum FileProviderLogDetail: Encodable {
    ///
    /// RFC 3339 formatter for the dates.
    ///
    static let dateFormatter = {
        let formatter = DateFormatter()

        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd'T'HH:mm:ssZZZZZ"
        formatter.timeZone = TimeZone(secondsFromGMT: 0)

        return formatter
    }()

    ///
    /// The represented detail value is a date but encoded as a formatted `String` in JSON.
    ///
    case date(Date)

    ///
    /// The represented detail value is a string in JSON.
    ///
    case string(String)

    ///
    /// The represented detail value is an integer in JSON.
    ///
    case int(Int)

    ///
    /// The represented detail value is a double in JSON.
    ///
    case double(Double)

    ///
    /// The represented detail value is a boolean in JSON.
    ///
    case bool(Bool)

    ///
    /// The represented detail value is an array in JSON.
    ///
    case array([FileProviderLogDetail])

    ///
    /// The represented detail value is a dictionary in JSON.
    ///
    case dictionary([String: FileProviderLogDetail])

    ///
    /// The represented detail value is `null` in JSON.
    ///
    case null

    var value: Any {
        switch self {
            case let .date(v):
                Self.dateFormatter.string(from: v)
            case let .string(v):
                v
            case let .int(v):
                v
            case let .double(v):
                v
            case let .bool(v):
                v
            case let .array(v):
                v.map(\.value)
            case let .dictionary(v):
                v.mapValues { $0.value }
            case .null:
                NSNull()
        }
    }

    ///
    /// Attempt to create a detail value based on any given type.
    ///
    init(_ anyOptional: Any?) {
        if let someValue = anyOptional {
            if someValue.self is String {
                self = .string(someValue as! String)
            } else if let date = someValue as? Date {
                self = .date(date)
            } else if let url = someValue as? URL {
                self = .string(url.absoluteString)
            } else if let account = someValue as? Account {
                self = .string(account.ncKitAccount)
            } else if let error = someValue as? NSFileProviderError {
                self = .string("NSFileProviderError.Code: \(error.code)")
            } else if let error = someValue as? NSError {
                self = .dictionary([
                    "code": .int(error.code),
                    "domain": .string(error.domain),
                    "localizedDescription": .string(error.localizedDescription)
                ])
            } else if let error = someValue as? Error {
                self = .string(error.localizedDescription)
            } else if let fileProviderDomainIdentifier = someValue as? NSFileProviderDomainIdentifier {
                self = .string(fileProviderDomainIdentifier.rawValue)
            } else if let item = someValue as? NSFileProviderItemProtocol {
                self = .string(item.itemIdentifier.rawValue)
            } else if let fileProviderItemIdentifier = someValue as? NSFileProviderItemIdentifier {
                self = .string(fileProviderItemIdentifier.rawValue)
            } else if let metadata = someValue as? SendableItemMetadata {
                self = .dictionary([
                    "account": .string(metadata.account),
                    "contentType": .string(metadata.contentType),
                    "creationDate": .date(metadata.creationDate),
                    "date": .date(metadata.date),
                    "deleted": .bool(metadata.deleted),
                    "directory": .bool(metadata.directory),
                    "downloaded": .bool(metadata.downloaded),
                    "eTag": .string(metadata.etag),
                    "keepDownloaded": .bool(metadata.keepDownloaded),
                    "lock": .bool(metadata.lock),
                    "lockTimeOut": metadata.lockTimeOut != nil ? .date(metadata.lockTimeOut!) : .string("nil"),
                    "lockOwner": metadata.lockOwner != nil ? .string(metadata.lockOwner!) : .null,
                    "name": .string(metadata.fileName),
                    "ocId": .string(metadata.ocId),
                    "permissions": .string(metadata.permissions),
                    "serverUrl": .string(metadata.serverUrl),
                    "size": .int(Int(metadata.size)),
                    "syncTime": .date(metadata.syncTime),
                    "trashbinFileName": .string(metadata.trashbinFileName),
                    "uploaded": .bool(metadata.uploaded),
                    "visitedDirectory": .bool(metadata.visitedDirectory)
                ])
            } else if let request = someValue as? NSFileProviderRequest {
                self = .dictionary([
                    "requestingExecutable": .string(request.requestingExecutable?.path ?? "nil"),
                    "isFileViewerRequest": .bool(request.isFileViewerRequest),
                    "isSystemRequest": .bool(request.isSystemRequest)
                ])
            } else if let lock = someValue as? NKLock {
                self = .dictionary([
                    "owner": .string(lock.owner),
                    "ownerDisplayName": .string(lock.ownerDisplayName),
                    "ownerEditor": lock.ownerEditor == nil ? .null : .string(lock.ownerEditor!),
                    "ownerType": .int(lock.ownerType.rawValue),
                    "time": lock.time == nil ? .null : .date(lock.time!),
                    "timeOut": lock.timeOut == nil ? .null : .date(lock.timeOut!),
                    "token": lock.token == nil ? .null : .string(lock.token!)
                ])
            } else {
                self = .string("Unsupported log detail type: \(String(describing: someValue.self))")
            }
        } else {
            self = .null
        }
    }

    // MARK: Encodable

    public func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()

        switch self {
            case let .date(v):
                try container.encode(v)
            case let .string(v):
                try container.encode(v)
            case let .int(v):
                try container.encode(v)
            case let .double(v):
                try container.encode(v)
            case let .bool(v):
                try container.encode(v)
            case let .array(v):
                try container.encode(v)
            case let .dictionary(v):
                try container.encode(v)
            case .null:
                try container.encodeNil()
        }
    }
}
