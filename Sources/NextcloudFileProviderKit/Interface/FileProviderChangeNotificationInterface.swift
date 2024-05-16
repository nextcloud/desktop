//
//  FileProviderChangeNotificationInterface.swift
//
//
//  Created by Claudio Cambra on 16/5/24.
//

import FileProvider
import Foundation
import OSLog

public class FileProviderChangeNotificationInterface: ChangeNotificationInterface {
    let domain: NSFileProviderDomain
    private let logger = Logger(
        subsystem: Logger.subsystem, category: "FileProviderChangeNotificationInterface"
    )

    required init(domain: NSFileProviderDomain) {
        self.domain = domain
    }

    public func notifyChange() {
        NSFileProviderManager(for: domain)?.signalEnumerator(for: .workingSet) { error in
            if let error {
                self.logger.error(
                    """
                    Could not signal enumerator for
                    \(self.domain.identifier.rawValue, privacy: .public):
                    \(error.localizedDescription, privacy: .public)
                    """
                )
            }
        }
    }
}
