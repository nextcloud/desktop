/*
 * Copyright (C) 2022 by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

import Foundation
import RealmSwift
import FileProvider

class NextcloudFilesDatabaseManager : NSObject {
    static let shared = {
        return NextcloudFilesDatabaseManager();
    }()

    let relativeDatabaseFolderPath: String = "FileProviderExt/Database/"
    let databaseFilename: String = "fileproviderextdatabase.realm"
    let relativeDatabaseFilePath: String
    var databasePath: URL?

    let schemaVersion: UInt64 = 100

    override init() {
        self.relativeDatabaseFilePath = self.relativeDatabaseFolderPath + self.databaseFilename

        guard let appGroupIdentifier = Bundle.main.object(forInfoDictionaryKey: "SocketApiPrefix") as? String else {
            super.init()
            return
        }

        let containerUrl = FileManager.default.containerURL(forSecurityApplicationGroupIdentifier: appGroupIdentifier)
        self.databasePath = containerUrl?.appendingPathExtension(self.relativeDatabaseFilePath)

        // Disable file protection for directory DB
        // https://docs.mongodb.com/realm/sdk/ios/examples/configure-and-open-a-realm/#std-label-ios-open-a-local-realm
        if let folderPathURL = containerUrl?.appendingPathComponent(self.relativeDatabaseFolderPath) {
            let folderPath = folderPathURL.path
            do {
                try FileManager.default.setAttributes([FileAttributeKey.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: folderPath)
            } catch {
                print("Could not set permission level for File Provider database folder")
            }
        }

        let config = Realm.Configuration(
            fileURL: self.databasePath,
            schemaVersion: self.schemaVersion,
            objectTypes: [NextcloudFileMetadataTable.self]
        )

        Realm.Configuration.defaultConfiguration = config

        do {
            let realm = try Realm()
            print("Successfully started Realm db for FileProviderExt")
        } catch let error as NSError {
            print("Error opening Realm db: %@", error.localizedDescription)
        }

        super.init()
    }

    func getFileMetadataFromOcId(ocId: String) -> NextcloudFileMetadataTable? {
        let realm = try! Realm()
        realm.refresh()
        return realm.objects(NextcloudFileMetadataTable.self).filter("ocId == %@", ocId).first
    }

    func getFileMetadataFromFileProviderItemIdentifier(identifier: NSFileProviderItemIdentifier) -> NextcloudFileMetadataTable? {
        let ocId = identifier.rawValue
        return getFileMetadataFromOcId(ocId: ocId)
    }
}
