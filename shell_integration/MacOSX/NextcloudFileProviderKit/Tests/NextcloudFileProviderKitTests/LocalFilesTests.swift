//  SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
//  SPDX-License-Identifier: LGPL-3.0-or-later

@testable import NextcloudFileProviderKit
import Testing

struct LocalFilesTests {
    @Test func recognisesLockFileNames() {
        // Microsoft Office.
        #expect(isLockFileName("~$Test.docx"))
        // LibreOffice.
        #expect(isLockFileName(".~lock.Test.odt#"))
        // Adobe InDesign / InCopy.
        #expect(isLockFileName("~Test~0kjyv(.idlk"))
        #expect(isLockFileName("Test.idlk"))
        #expect(isLockFileName("Test.IDLK")) // Case-insensitive extension.
        // Adobe Premiere Pro.
        #expect(isLockFileName("Test.prlock"))

        // Guarded documents themselves are not lock files.
        #expect(!isLockFileName("Test.indd"))
        #expect(!isLockFileName("Test.icml"))
        #expect(!isLockFileName("Test.prproj"))
        #expect(!isLockFileName("Test.docx"))
        #expect(!isLockFileName("Test.odt"))
    }

    @Test func recognisesAdobeLockFileNames() {
        #expect(isAdobeLockFileName("~Test~0kjyv(.idlk"))
        #expect(isAdobeLockFileName("Test.prlock"))
        #expect(isAdobeLockFileName("Test.IdLk")) // Case-insensitive extension.

        #expect(!isAdobeLockFileName("~$Test.docx")) // Microsoft Office is not Adobe.
        #expect(!isAdobeLockFileName(".~lock.Test.odt#")) // LibreOffice is not Adobe.
        #expect(!isAdobeLockFileName("Test.indd"))
        #expect(!isAdobeLockFileName("Test.prproj"))
    }

    @Test func extractsAdobeDocumentBaseName() {
        // InDesign / InCopy with a random token.
        #expect(adobeLockFileDocumentBaseName("~Test~0kjyv(.idlk") == "Test")
        // InDesign / InCopy without a token.
        #expect(adobeLockFileDocumentBaseName("~Test(.idlk") == "Test")
        // Base name containing a tilde is preserved (only the trailing token is dropped).
        #expect(adobeLockFileDocumentBaseName("~My~Doc~0kjyv(.idlk") == "My~Doc")
        // Premiere Pro.
        #expect(adobeLockFileDocumentBaseName("Test.prlock") == "Test")

        // Non-Adobe extensions yield no base name.
        #expect(adobeLockFileDocumentBaseName("Test.docx") == nil)
        #expect(adobeLockFileDocumentBaseName("~$Test.docx") == nil)
    }
}
