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
import NCDesktopClientSocketKit

class FileProviderSocketLineProcessor: NSObject, LineProcessor {
    var delegate: FileProviderExtension

    init(delegate: FileProviderExtension) {
        self.delegate = delegate
    }

    func process(_ line: String) {
        NSLog("Processing file provider line: %@", line)

        let splitLine = line.split(separator: ":")
        guard let commandSubsequence = splitLine.first else {
            NSLog("Input line did not have a first element")
            return;
        }
        let command = String(commandSubsequence);

        NSLog("Received command: %@", command)
        if (command == "SEND_FILE_PROVIDER_DOMAIN_IDENTIFIER") {
            delegate.sendDelegateFileProviderDomainIdentifier()
        }
    }
}
