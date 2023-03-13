/*
 * Copyright (C) 2023 by Claudio Cambra <claudio.cambra@nextcloud.com>
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

class NextcloudDirectoryMetadataTable: Object {
    func isInSameRemoteState(_ comparingMetadata: NextcloudDirectoryMetadataTable) -> Bool {
        return comparingMetadata.etag == self.etag &&
            comparingMetadata.e2eEncrypted == self.e2eEncrypted &&
            comparingMetadata.favorite == self.favorite &&
            comparingMetadata.permissions == self.permissions
    }

    @Persisted(primaryKey: true) var ocId: String
    @Persisted var account = ""
    @Persisted var colorFolder: String?
    @Persisted var e2eEncrypted: Bool = false
    @Persisted var etag = ""
    @Persisted var favorite: Bool = false
    @Persisted var fileId = ""
    @Persisted var offline: Bool = false
    @Persisted var permissions = ""
    @Persisted var richWorkspace: String?
    @Persisted var serverUrl = ""
    @Persisted var parentDirectoryServerUrl = ""
}
