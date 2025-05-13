//
//  FilesDatabaseManager+KeepOffline.swift
//  NextcloudFileProviderKit
//
//  Created by Claudio Cambra on 13/5/25.
//

import Foundation
import RealmSwift

public extension FilesDatabaseManager {
    func set(
        keepOffline: Bool, for metadata: SendableItemMetadata
    ) throws -> SendableItemMetadata? {
        guard #available(macOS 13.0, iOS 16.0, visionOS 1.0, *) else {
            let errorString = """
                Could not update keepOffline status for item: \(metadata.fileName)
                    as the system does not support this state.
            """
            Self.logger.error("\(errorString, privacy: .public)")
            throw NSError(
                domain: Self.errorDomain,
                code: NSFeatureUnsupportedError,
                userInfo: [NSLocalizedDescriptionKey: errorString]
            )
        }

        guard let result = itemMetadatas.where({ $0.ocId == metadata.ocId }).first else {
            let errorString = """
                Did not update keepOffline for item metadata as it was not found.
                    ocID: \(metadata.ocId)
                    filename: \(metadata.fileName)
            """
            Self.logger.error("\(errorString, privacy: .public)")
            throw NSError(
                domain: Self.errorDomain,
                code: ErrorCode.metadataNotFound.rawValue,
                userInfo: [NSLocalizedDescriptionKey: errorString]
            )
        }

        try ncDatabase().write {
            result.keepOffline = keepOffline

            Self.logger.debug(
                """
                Updated keepOffline status for item metadata.
                    ocID: \(metadata.ocId, privacy: .public)
                    fileName: \(metadata.fileName, privacy: .public)
                """
            )
        }
        return SendableItemMetadata(value: result)
    }
}
