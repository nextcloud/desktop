//  SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@preconcurrency import FileProvider
import Foundation
import NextcloudKit

extension NKError {
    static var noChangesErrorCode: Int {
        -200
    }

    /// The metadata database could not be read, so the operation must not proceed on a guess. Unlisted below, so it maps to `cannotSynchronize`.
    static let metadataDatabaseUnreadable = NKError(errorCode: -2000, errorDescription: "The metadata database could not be read.")

    /// A read of a parked folder's holding path, which exists only in the database, and unlisted below, so it maps to `cannotSynchronize`.
    static let parkedFolderPath = NKError(errorCode: -2001, errorDescription: "The path belongs to a parked folder.")

    /// Local persistence failed. Maps to `cannotSynchronize`, not a server or quota error.
    static let metadataDatabaseWriteFailed = NKError(errorCode: NSFileProviderError.cannotSynchronize.rawValue, errorDescription: "The metadata database could not be updated.")

    var isCancelledError: Bool {
        errorCode == NSURLErrorCancelled
    }

    var isChunkingCancellationError: Bool {
        // NextcloudKit uses -5 in the Cocoa domain for cancelled preparation and in the chunkedFile domain for empty chunks.
        errorCode == -5 && (error as NSError).domain == NSCocoaErrorDomain
    }

    /// Server errors a retry cannot fix: 501 Not Implemented, 505 HTTP Version Not Supported, and 507 Insufficient
    /// Storage, which maps to `insufficientQuota` instead.
    ///
    /// Every other 5xx status counts as a temporary outage. Proxies, CDNs and hosters return many non-standard 5xx
    /// statuses when the server behind them is slow, overloaded or unreachable (Cloudflare's 520 to 530, CloudLinux's
    /// 508, 598 and 599), so no list of them is complete.
    static let permanentServerErrorStatusCodes: Set<Int> = [501, 505, 507]

    /// Client error statuses that a proxy or the server returns while it is slow or busy:
    /// 408 Request Timeout and 429 Too Many Requests.
    static let temporaryClientErrorStatusCodes: Set<Int> = [408, 429]

    var isCouldntConnectError: Bool {
        errorCode == -9999 || errorCode == -1001 || errorCode == -1004 || errorCode == -1005
            || errorCode == -1009 || errorCode == -1012 || errorCode == -1200 || errorCode == -1202
            || errorCode == 200 || Self.temporaryClientErrorStatusCodes.contains(errorCode)
            || ((500 ... 599).contains(errorCode) && !Self.permanentServerErrorStatusCodes.contains(errorCode))
    }

    var isUnauthenticatedError: Bool {
        errorCode == -1013
    }

    /// The local disk has no room for an upload: NextcloudKit's free space check or writing the chunks failed.
    var isLocalStorageFullError: Bool {
        kind == .localStorageFull
    }

    var isGoingOverQuotaError: Bool {
        errorCode == 507
    }

    var isNotFoundError: Bool {
        errorCode == 404
    }

    var isPreconditionFailedError: Bool {
        errorCode == 412
    }

    var isLockedError: Bool {
        errorCode == 423
    }

    var isNoChangesError: Bool {
        errorCode == NKError.noChangesErrorCode
    }

    var isUnauthorizedError: Bool {
        errorCode == 401
    }

    var matchesCollisionError: Bool {
        errorCode == 405
    }

    ///
    /// Derive an `NSFileProviderError` based on ``errorCode``.
    ///
    var fileProviderError: NSFileProviderError? {
        if self == .success {
            nil
        } else if isNotFoundError {
            NSFileProviderError(.noSuchItem)
        } else if isCouldntConnectError {
            NSFileProviderError(.serverUnreachable)
        } else if isUnauthenticatedError || isUnauthorizedError {
            NSFileProviderError(.notAuthenticated)
        } else if isGoingOverQuotaError {
            NSFileProviderError(.insufficientQuota)
        } else if matchesCollisionError {
            NSFileProviderError(.filenameCollision)
        } else {
            NSFileProviderError(.cannotSynchronize)
        }
    }

    func fileProviderError(
        handlingNoSuchItemErrorUsingItemIdentifier identifier: NSFileProviderItemIdentifier
    ) -> Error? {
        if isCancelledError {
            return CocoaError(.userCancelled)
        }
        guard fileProviderError?.code == .noSuchItem else {
            return fileProviderError as Error?
        }
        return NSError.fileProviderErrorForNonExistentItem(withIdentifier: identifier)
    }

    func fileProviderError(
        handlingCollisionAgainstItemInRemotePath problemRemotePath: String,
        dbManager: FilesDatabaseManager,
        remoteInterface: RemoteInterface,
        log: any FileProviderLogging
    ) async -> Error? {
        if isCancelledError {
            return CocoaError(.userCancelled)
        }
        if isLocalStorageFullError {
            return CocoaError(.fileWriteOutOfSpace)
        }
        guard fileProviderError?.code == .filenameCollision else {
            return fileProviderError as Error?
        }
        guard let collidingItemMetadata = dbManager.itemMetadata(
            account: dbManager.account.ncKitAccount, locatedAtRemoteUrl: problemRemotePath
        ), let collidingItem = await Item.storedItem(
            identifier: .init(collidingItemMetadata.ocId),
            account: dbManager.account,
            remoteInterface: remoteInterface,
            dbManager: dbManager,
            log: log
        ) else {
            return NSFileProviderError(.filenameCollision)
        }
        return NSError.fileProviderErrorForCollision(with: collidingItem)
    }
}
